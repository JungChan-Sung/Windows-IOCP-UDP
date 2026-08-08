#include "PlayerRepositoryIntegrationTests.h"

#include <Windows.h>

#include <expected>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <Persistence/Account/AccountRepository.h>
#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>
#include <Persistence/Odbc/OdbcEnvironment.h>
#include <Persistence/Odbc/OdbcStatement.h>
#include <Persistence/Player/PlayerRepository.h>
#include <Persistence/Schema/DatabaseSchema.h>

#include <Tests/DebugTestResult.h>

namespace
{
	inline constexpr std::string_view databaseConnectionStringEnvironmentName = "WINDOWS_IOCP_UDP_TEST_DB_CONNECTION_STRING";

	using DeleteAccountResult = std::expected<void, persistence::core::DatabaseError>;

	[[nodiscard]] std::optional<std::string> ReadDatabaseConnectionString()
	{
		const DWORD requiredSize = ::GetEnvironmentVariableA(databaseConnectionStringEnvironmentName.data(), nullptr, 0);
		if (requiredSize == 0)
		{
			return std::nullopt;
		}

		std::string value(requiredSize, '\0');

		const DWORD copiedSize = ::GetEnvironmentVariableA(databaseConnectionStringEnvironmentName.data(), value.data(), requiredSize);
		if (copiedSize == 0 || copiedSize >= requiredSize)
		{
			return std::nullopt;
		}

		value.resize(copiedSize);
		return value;
	}

	void AddDatabaseFailure(tests::DebugTestResult& result, std::string_view operationName, const persistence::core::DatabaseError& error)
	{
		std::string message(operationName);
		message += ": ";
		message += persistence::core::ToString(error);

		result.AddFailed(message);
	}

	[[nodiscard]] DeleteAccountResult DeleteAccountByLoginName(persistence::odbc::OdbcConnection& connection, std::string_view loginName)
	{
		persistence::odbc::OdbcStatement statement;

		const persistence::odbc::OdbcStatement::ExecuteResult prepareResult = statement.Prepare(
			connection,
			R"sql(
DELETE FROM dbo.accounts
WHERE login_name = ?;
)sql"
);
		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const persistence::odbc::OdbcStatement::BindResult bindResult
			= statement.BindInputString(static_cast<SQLUSMALLINT>(1), loginName);

		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const persistence::odbc::OdbcStatement::ExecuteResult executeResult = statement.Execute();
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		return {};
	}

	void RunPlayerRoundTripTest(
		tests::DebugTestResult& result,
		persistence::account::AccountRepository& accountRepository,
		persistence::player::PlayerRepository& playerRepository,
		persistence::odbc::OdbcConnection& connection
	)
	{
		constexpr std::string_view loginName = "player_repository_integration_test";

		const DeleteAccountResult initialDeleteResult = DeleteAccountByLoginName(connection, loginName);
		if (!initialDeleteResult.has_value())
		{
			AddDatabaseFailure(result, "PlayerRepositoryIntegration: initial cleanup", initialDeleteResult.error());
			return;
		}

		const persistence::account::AccountRepository::CreateAccountResult accountCreateResult = accountRepository.CreateAccount(
			persistence::account::AccountCreateRequest{
				.loginName = loginName,
				.passwordHash = "player_repository_hash",
				.nickname = "PlayerRepositoryTester",
			}
			);
		if (!accountCreateResult.has_value())
		{
			AddDatabaseFailure(result, "PlayerRepositoryIntegration: account creation", accountCreateResult.error());
			return;
		}

		const std::int64_t accountId = accountCreateResult->accountId;

		const persistence::player::PlayerRepository::FindPlayerResult initialFindResult = playerRepository.FindPlayerByAccountId(accountId);
		if (!initialFindResult.has_value())
		{
			AddDatabaseFailure(result, "PlayerRepositoryIntegration: initial player lookup", initialFindResult.error());
			return;
		}

		tests::Expect(result, !initialFindResult->has_value(), "PlayerRepositoryIntegration: player does not exist before creation");

		const persistence::player::PlayerRepository::CreatePlayerResult createResult = playerRepository.CreatePlayer(accountId);
		if (!createResult.has_value())
		{
			AddDatabaseFailure(result, "PlayerRepositoryIntegration: player creation", createResult.error());
			return;
		}

		tests::Expect(result, createResult->playerId > 0, "PlayerRepositoryIntegration: created player id is valid");
		tests::Expect(result, createResult->accountId == accountId, "PlayerRepositoryIntegration: created account id matches");

		const persistence::player::PlayerRepository::FindPlayerResult findResult = playerRepository.FindPlayerByAccountId(accountId);
		if (!findResult.has_value())
		{
			AddDatabaseFailure(result, "PlayerRepositoryIntegration: player lookup", findResult.error());
			return;
		}

		tests::Expect(result, findResult->has_value(), "PlayerRepositoryIntegration: created player can be found");

		if (findResult->has_value())
		{
			const persistence::player::PlayerRecord& player = **findResult;

			tests::Expect(result, player.playerId == createResult->playerId, "PlayerRepositoryIntegration: found player id matches");
			tests::Expect(result, player.accountId == accountId, "PlayerRepositoryIntegration: found account id matches");
		}

		const persistence::player::PlayerRepository::CreatePlayerResult duplicateCreateResult = playerRepository.CreatePlayer(accountId);

		tests::Expect(result, !duplicateCreateResult.has_value(), "PlayerRepositoryIntegration: duplicate player creation rejected");

		if (!duplicateCreateResult.has_value())
		{
			const bool duplicateConstraintDetected
				= persistence::core::ContainsNativeError(duplicateCreateResult.error(), 2601)
				|| persistence::core::ContainsNativeError(duplicateCreateResult.error(), 2627);

			tests::Expect(result, duplicateConstraintDetected, "PlayerRepositoryIntegration: duplicate constraint error reported");
		}

		const DeleteAccountResult finalDeleteResult = DeleteAccountByLoginName(connection, loginName);
		if (!finalDeleteResult.has_value())
		{
			AddDatabaseFailure(result, "PlayerRepositoryIntegration: final cleanup", finalDeleteResult.error());
			return;
		}

		const persistence::player::PlayerRepository::FindPlayerResult findAfterDeleteResult = playerRepository.FindPlayerByAccountId(accountId);
		if (!findAfterDeleteResult.has_value())
		{
			AddDatabaseFailure(result, "PlayerRepositoryIntegration: lookup after account deletion", findAfterDeleteResult.error());
			return;
		}

		tests::Expect(result, !findAfterDeleteResult->has_value(), "PlayerRepositoryIntegration: account deletion cascades to player");
	}
}

namespace tests::persistence
{
	DebugTestResult RunPlayerRepositoryIntegrationTests()
	{
		DebugTestResult result{};

		const std::optional<std::string> connectionString = ReadDatabaseConnectionString();
		if (!connectionString.has_value())
		{
			std::cout << "[PlayerRepositoryIntegration] Skipped: " << databaseConnectionStringEnvironmentName << " is not set.\n";
			return result;
		}

		::persistence::odbc::OdbcEnvironment environment;

		const ::persistence::odbc::OdbcEnvironment::InitializeResult initializeResult = environment.Initialize();
		if (!initializeResult.has_value())
		{
			AddDatabaseFailure(result, "PlayerRepositoryIntegration: environment initialization", initializeResult.error());
			return result;
		}

		::persistence::odbc::OdbcConnection connection;

		const ::persistence::odbc::OdbcConnection::OpenResult openResult = connection.Open(
			environment,
			::persistence::odbc::OdbcConnectionOpenConfig{
				.connectionString = *connectionString,
				.connectionTimeoutSeconds = 5,
			}
			);
		if (!openResult.has_value())
		{
			AddDatabaseFailure(result, "PlayerRepositoryIntegration: database connection", openResult.error());
			return result;
		}

		const ::persistence::schema::DatabaseSchema::InitializeResult schemaResult = ::persistence::schema::DatabaseSchema::Initialize(connection);
		if (!schemaResult.has_value())
		{
			AddDatabaseFailure(result, "PlayerRepositoryIntegration: schema initialization", schemaResult.error());
			return result;
		}

		::persistence::account::AccountRepository accountRepository(connection);
		::persistence::player::PlayerRepository playerRepository(connection);

		RunPlayerRoundTripTest(result, accountRepository, playerRepository, connection);

		return result;
	}
}