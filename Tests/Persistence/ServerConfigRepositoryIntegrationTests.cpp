#include "ServerConfigRepositoryIntegrationTests.h"

#include <Windows.h>

#include <algorithm>
#include <expected>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <Persistence/Config/ServerConfigRepository.h>
#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>
#include <Persistence/Odbc/OdbcEnvironment.h>
#include <Persistence/Odbc/OdbcStatement.h>
#include <Persistence/Schema/DatabaseSchema.h>

#include <Tests/DebugTestResult.h>

namespace
{
	inline constexpr std::string_view databaseConnectionStringEnvironmentName
		= "WINDOWS_IOCP_UDP_TEST_DB_CONNECTION_STRING";

	inline constexpr std::string_view peerTimeoutKey
		= "Tests.Session.PeerTimeoutSeconds";

	inline constexpr std::string_view logLevelKey
		= "Tests.Diagnostics.LogLevel";

	using DatabaseOperationResult
		= std::expected<void, persistence::core::DatabaseError>;

	[[nodiscard]] std::optional<std::string> ReadDatabaseConnectionString()
	{
		const DWORD requiredSize = ::GetEnvironmentVariableA(
			databaseConnectionStringEnvironmentName.data(),
			nullptr,
			0
		);

		if (requiredSize == 0)
		{
			return std::nullopt;
		}

		std::string value(requiredSize, '\0');

		const DWORD copiedSize = ::GetEnvironmentVariableA(
			databaseConnectionStringEnvironmentName.data(),
			value.data(),
			requiredSize
		);

		if (copiedSize == 0 || copiedSize >= requiredSize)
		{
			return std::nullopt;
		}

		value.resize(copiedSize);
		return value;
	}

	void AddDatabaseFailure(
		tests::DebugTestResult& result,
		std::string_view operationName,
		const persistence::core::DatabaseError& error
	)
	{
		std::string message(operationName);
		message += ": ";
		message += persistence::core::ToString(error);

		result.AddFailed(message);
	}

	[[nodiscard]] DatabaseOperationResult DeleteConfigEntry(
		persistence::odbc::OdbcConnection& connection,
		std::string_view key
	)
	{
		persistence::odbc::OdbcStatement statement;

		const persistence::odbc::OdbcStatement::ExecuteResult prepareResult
			= statement.Prepare(
				connection,
				R"sql(
DELETE FROM dbo.server_configs
WHERE config_key = ?;
)sql"
);

		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const persistence::odbc::OdbcStatement::BindResult bindResult
			= statement.BindInputString(
				static_cast<SQLUSMALLINT>(1),
				key
			);

		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const persistence::odbc::OdbcStatement::ExecuteResult executeResult
			= statement.Execute();

		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		return {};
	}

	[[nodiscard]] DatabaseOperationResult InsertConfigEntry(
		persistence::odbc::OdbcConnection& connection,
		std::string_view key,
		std::string_view value
	)
	{
		persistence::odbc::OdbcStatement statement;

		const persistence::odbc::OdbcStatement::ExecuteResult prepareResult
			= statement.Prepare(
				connection,
				R"sql(
INSERT INTO dbo.server_configs
(
	config_key,
	config_value
)
VALUES
(
	?,
	?
);
)sql"
);

		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const persistence::odbc::OdbcStatement::BindResult keyBindResult
			= statement.BindInputString(
				static_cast<SQLUSMALLINT>(1),
				key
			);

		if (!keyBindResult.has_value())
		{
			return std::unexpected(keyBindResult.error());
		}

		const persistence::odbc::OdbcStatement::BindResult valueBindResult
			= statement.BindInputString(
				static_cast<SQLUSMALLINT>(2),
				value
			);

		if (!valueBindResult.has_value())
		{
			return std::unexpected(valueBindResult.error());
		}

		const persistence::odbc::OdbcStatement::ExecuteResult executeResult
			= statement.Execute();

		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		return {};
	}

	[[nodiscard]] DatabaseOperationResult CleanupTestEntries(
		persistence::odbc::OdbcConnection& connection
	)
	{
		const DatabaseOperationResult peerTimeoutDeleteResult
			= DeleteConfigEntry(connection, peerTimeoutKey);

		if (!peerTimeoutDeleteResult.has_value())
		{
			return std::unexpected(peerTimeoutDeleteResult.error());
		}

		const DatabaseOperationResult logLevelDeleteResult
			= DeleteConfigEntry(connection, logLevelKey);

		if (!logLevelDeleteResult.has_value())
		{
			return std::unexpected(logLevelDeleteResult.error());
		}

		return {};
	}

	void RunLoadAllTest(
		tests::DebugTestResult& result,
		persistence::config::ServerConfigRepository& repository,
		persistence::odbc::OdbcConnection& connection
	)
	{
		const DatabaseOperationResult initialCleanupResult
			= CleanupTestEntries(connection);

		if (!initialCleanupResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"ServerConfigRepositoryIntegration: initial cleanup",
				initialCleanupResult.error()
			);

			return;
		}

		const DatabaseOperationResult peerTimeoutInsertResult
			= InsertConfigEntry(
				connection,
				peerTimeoutKey,
				"15"
			);

		if (!peerTimeoutInsertResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"ServerConfigRepositoryIntegration: peer timeout insert",
				peerTimeoutInsertResult.error()
			);

			return;
		}

		const DatabaseOperationResult logLevelInsertResult
			= InsertConfigEntry(
				connection,
				logLevelKey,
				"Debug"
			);

		if (!logLevelInsertResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"ServerConfigRepositoryIntegration: log level insert",
				logLevelInsertResult.error()
			);

			static_cast<void>(CleanupTestEntries(connection));
			return;
		}

		const persistence::config::ServerConfigRepository::LoadAllResult loadResult
			= repository.LoadAll();

		if (!loadResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"ServerConfigRepositoryIntegration: load all",
				loadResult.error()
			);

			static_cast<void>(CleanupTestEntries(connection));
			return;
		}

		const auto peerTimeoutIterator = std::ranges::find_if(
			*loadResult,
			[](const persistence::config::ServerConfigEntry& entry)
			{
				return entry.key == peerTimeoutKey;
			}
		);

		tests::Expect(
			result,
			peerTimeoutIterator != loadResult->end(),
			"ServerConfigRepositoryIntegration: peer timeout entry loaded"
		);

		if (peerTimeoutIterator != loadResult->end())
		{
			tests::Expect(
				result,
				peerTimeoutIterator->value == "15",
				"ServerConfigRepositoryIntegration: peer timeout value matches"
			);
		}

		const auto logLevelIterator = std::ranges::find_if(
			*loadResult,
			[](const persistence::config::ServerConfigEntry& entry)
			{
				return entry.key == logLevelKey;
			}
		);

		tests::Expect(
			result,
			logLevelIterator != loadResult->end(),
			"ServerConfigRepositoryIntegration: log level entry loaded"
		);

		if (logLevelIterator != loadResult->end())
		{
			tests::Expect(
				result,
				logLevelIterator->value == "Debug",
				"ServerConfigRepositoryIntegration: log level value matches"
			);
		}

		const DatabaseOperationResult finalCleanupResult
			= CleanupTestEntries(connection);

		if (!finalCleanupResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"ServerConfigRepositoryIntegration: final cleanup",
				finalCleanupResult.error()
			);
		}
	}
}

namespace tests::persistence
{
	DebugTestResult RunServerConfigRepositoryIntegrationTests()
	{
		DebugTestResult result{};

		const std::optional<std::string> connectionString
			= ReadDatabaseConnectionString();

		if (!connectionString.has_value())
		{
			std::cout
				<< "[ServerConfigRepositoryIntegration] Skipped: "
				<< databaseConnectionStringEnvironmentName
				<< " is not set.\n";

			return result;
		}

		::persistence::odbc::OdbcEnvironment environment;

		const ::persistence::odbc::OdbcEnvironment::InitializeResult initializeResult
			= environment.Initialize();

		if (!initializeResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"ServerConfigRepositoryIntegration: environment initialization",
				initializeResult.error()
			);

			return result;
		}

		::persistence::odbc::OdbcConnection connection;

		const ::persistence::odbc::OdbcConnection::OpenResult openResult
			= connection.Open(
				environment,
				::persistence::odbc::OdbcConnectionOpenConfig{
					.connectionString = *connectionString,
					.connectionTimeoutSeconds = 5,
				}
				);

		if (!openResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"ServerConfigRepositoryIntegration: database connection",
				openResult.error()
			);

			return result;
		}

		const ::persistence::schema::DatabaseSchema::InitializeResult schemaResult
			= ::persistence::schema::DatabaseSchema::Initialize(connection);

		if (!schemaResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"ServerConfigRepositoryIntegration: schema initialization",
				schemaResult.error()
			);

			return result;
		}

		::persistence::config::ServerConfigRepository repository(connection);

		RunLoadAllTest(
			result,
			repository,
			connection
		);

		return result;
	}
}