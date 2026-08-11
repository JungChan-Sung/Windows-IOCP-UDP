#include "PersistenceRuntimeIntegrationTests.h"

#include <Windows.h>

#include <expected>
#include <future>
#include <iostream>
#include <latch>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Core/PersistenceRuntime.h>
#include <Persistence/Odbc/OdbcConnection.h>
#include <Persistence/Odbc/OdbcEnvironment.h>
#include <Persistence/Odbc/OdbcStatement.h>

#include <Tests/DebugTestResult.h>

namespace
{
	inline constexpr std::string_view databaseConnectionStringEnvironmentName = "WINDOWS_IOCP_UDP_TEST_DB_CONNECTION_STRING";
	inline constexpr std::string_view playerTestLoginName = "persistence_runtime_player_test";

	using WorkerResult = std::expected<void, ::persistence::core::DatabaseError>;
	using DeleteAccountResult = std::expected<void, ::persistence::core::DatabaseError>;

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

	void AddDatabaseFailure(tests::DebugTestResult& result, std::string_view operationName, const ::persistence::core::DatabaseError& error)
	{
		std::string message(operationName);
		message += ": ";
		message += ::persistence::core::ToString(error);

		result.AddFailed(message);
	}

	[[nodiscard]] DeleteAccountResult DeleteAccountByLoginName(::persistence::odbc::OdbcConnection& connection, std::string_view loginName)
	{
		::persistence::odbc::OdbcStatement statement;

		const ::persistence::odbc::OdbcStatement::ExecuteResult prepareResult = statement.Prepare(
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

		const ::persistence::odbc::OdbcStatement::BindResult bindResult = statement.BindInputString(1, loginName);
		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const ::persistence::odbc::OdbcStatement::ExecuteResult executeResult = statement.Execute();
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		return {};
	}

	[[nodiscard]] WorkerResult RunConcurrentExistsWorker(
		::persistence::PersistenceRuntime& persistenceRuntime,
		std::latch& startSignal,
		std::size_t operationCount
	)
	{
		startSignal.wait();

		for (std::size_t index = 0; index < operationCount; ++index)
		{
			const ::persistence::PersistenceRuntime::ExistsAccountResult existsResult
				= persistenceRuntime.ExistsByLoginName("persistence_runtime_concurrent_missing_account");

			if (!existsResult.has_value())
			{
				return std::unexpected(existsResult.error());
			}
		}

		return {};
	}

	void RunFindOrCreatePlayerTest(
		tests::DebugTestResult& result,
		::persistence::PersistenceRuntime& persistenceRuntime,
		::persistence::odbc::OdbcConnection& cleanupConnection
	)
	{
		const DeleteAccountResult initialDeleteResult = DeleteAccountByLoginName(cleanupConnection, playerTestLoginName);
		if (!initialDeleteResult.has_value())
		{
			AddDatabaseFailure(result, "PersistenceRuntimeIntegration: initial player account cleanup failed", initialDeleteResult.error());
			return;
		}

		const ::persistence::PersistenceRuntime::CreateAccountResult createAccountResult = persistenceRuntime.CreateAccount(
			::persistence::account::AccountCreateRequest{
				.loginName = playerTestLoginName,
				.passwordHash = "persistence_runtime_player_hash",
				.nickname = "PersistenceRuntimePlayer",
			}
			);

		if (!createAccountResult.has_value())
		{
			AddDatabaseFailure(result, "PersistenceRuntimeIntegration: player test account creation failed", createAccountResult.error());
			return;
		}

		const std::int64_t accountId = createAccountResult->accountId;

		const ::persistence::PersistenceRuntime::FindPlayerResult initialFindResult = persistenceRuntime.FindPlayerByAccountId(accountId);
		if (!initialFindResult.has_value())
		{
			AddDatabaseFailure(result, "PersistenceRuntimeIntegration: initial player lookup failed", initialFindResult.error());
			return;
		}

		tests::Expect(result, !initialFindResult->has_value(), "PersistenceRuntimeIntegration: persistent player initially missing");

		const ::persistence::PersistenceRuntime::FindOrCreatePlayerResult firstResult
			= persistenceRuntime.FindOrCreatePlayerByAccountId(accountId);

		if (!firstResult.has_value())
		{
			AddDatabaseFailure(result, "PersistenceRuntimeIntegration: first find-or-create failed", firstResult.error());
			return;
		}

		tests::Expect(result, firstResult->playerId > 0, "PersistenceRuntimeIntegration: find-or-create returns valid player id");
		tests::Expect(result, firstResult->accountId == accountId, "PersistenceRuntimeIntegration: find-or-create account id matches");

		const ::persistence::PersistenceRuntime::FindOrCreatePlayerResult secondResult
			= persistenceRuntime.FindOrCreatePlayerByAccountId(accountId);

		if (!secondResult.has_value())
		{
			AddDatabaseFailure(result, "PersistenceRuntimeIntegration: second find-or-create failed", secondResult.error());
			return;
		}

		tests::Expect(result, secondResult->playerId == firstResult->playerId, "PersistenceRuntimeIntegration: repeated find-or-create preserves player id");
		tests::Expect(result, secondResult->accountId == accountId, "PersistenceRuntimeIntegration: repeated find-or-create account id matches");

		const ::persistence::PersistenceRuntime::FindPlayerResult findResult = persistenceRuntime.FindPlayerByAccountId(accountId);
		if (!findResult.has_value())
		{
			AddDatabaseFailure(result, "PersistenceRuntimeIntegration: final player lookup failed", findResult.error());
			return;
		}

		tests::Expect(result, findResult->has_value(), "PersistenceRuntimeIntegration: find-or-create persists player");

		if (findResult->has_value())
		{
			tests::Expect(result, (**findResult).playerId == firstResult->playerId, "PersistenceRuntimeIntegration: persisted player id matches");
			tests::Expect(result, (**findResult).accountId == accountId, "PersistenceRuntimeIntegration: persisted account id matches");
		}

		const DeleteAccountResult finalDeleteResult = DeleteAccountByLoginName(cleanupConnection, playerTestLoginName);
		if (!finalDeleteResult.has_value())
		{
			AddDatabaseFailure(result, "PersistenceRuntimeIntegration: final player account cleanup failed", finalDeleteResult.error());
		}
	}
}

namespace tests::persistence
{
	DebugTestResult RunPersistenceRuntimeIntegrationTests()
	{
		DebugTestResult result{};

		const std::optional<std::string> connectionString = ReadDatabaseConnectionString();
		if (!connectionString.has_value())
		{
			std::cout << "[PersistenceRuntimeIntegration] Skipped: " << databaseConnectionStringEnvironmentName << " is not set.\n";
			return result;
		}

		::persistence::PersistenceRuntime persistenceRuntime;

		const ::persistence::PersistenceRuntime::StartResult startResult = persistenceRuntime.Start(
			::persistence::PersistenceRuntimeStartConfig{
				.enabled = true,
				.connectionString = *connectionString,
				.connectionTimeoutSeconds = 5,
			}
			);

		if (!startResult.has_value())
		{
			AddDatabaseFailure(result, "PersistenceRuntimeIntegration: start failed", startResult.error());
			return result;
		}

		tests::Expect(result, persistenceRuntime.IsEnabled(), "PersistenceRuntimeIntegration: enabled after start");
		tests::Expect(result, persistenceRuntime.IsStarted(), "PersistenceRuntimeIntegration: started before concurrent access");

		::persistence::odbc::OdbcEnvironment cleanupEnvironment;

		const ::persistence::odbc::OdbcEnvironment::InitializeResult initializeResult = cleanupEnvironment.Initialize();
		if (!initializeResult.has_value())
		{
			AddDatabaseFailure(result, "PersistenceRuntimeIntegration: cleanup environment initialization failed", initializeResult.error());
			persistenceRuntime.Stop();
			return result;
		}

		::persistence::odbc::OdbcConnection cleanupConnection;

		const ::persistence::odbc::OdbcConnection::OpenResult openResult = cleanupConnection.Open(
			cleanupEnvironment,
			::persistence::odbc::OdbcConnectionOpenConfig{
				.connectionString = *connectionString,
				.connectionTimeoutSeconds = 5,
			}
			);

		if (!openResult.has_value())
		{
			AddDatabaseFailure(result, "PersistenceRuntimeIntegration: cleanup connection failed", openResult.error());
			persistenceRuntime.Stop();
			return result;
		}

		RunFindOrCreatePlayerTest(result, persistenceRuntime, cleanupConnection);

		constexpr std::size_t workerCount = 8;
		constexpr std::size_t operationCountPerWorker = 8;

		std::latch startSignal{ 1 };

		std::vector<std::future<WorkerResult>> workers;
		workers.reserve(workerCount);

		for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
		{
			workers.push_back(
				std::async(
					std::launch::async,
					[&persistenceRuntime, &startSignal]()
					{
						return RunConcurrentExistsWorker(persistenceRuntime, startSignal, operationCountPerWorker);
					}
				)
			);
		}

		startSignal.count_down();

		std::optional<::persistence::core::DatabaseError> firstWorkerError;

		for (std::future<WorkerResult>& worker : workers)
		{
			WorkerResult workerResult = worker.get();

			if (!workerResult.has_value() && !firstWorkerError.has_value())
			{
				firstWorkerError = workerResult.error();
			}
		}

		std::string concurrentAccessMessage = "PersistenceRuntimeIntegration: concurrent database operations succeed";

		if (firstWorkerError.has_value())
		{
			concurrentAccessMessage += ": ";
			concurrentAccessMessage += ::persistence::core::ToString(*firstWorkerError);
		}

		tests::Expect(result, !firstWorkerError.has_value(), concurrentAccessMessage);
		tests::Expect(result, persistenceRuntime.IsStarted(), "PersistenceRuntimeIntegration: remains started after concurrent access");

		persistenceRuntime.Stop();

		tests::Expect(result, !persistenceRuntime.IsEnabled(), "PersistenceRuntimeIntegration: disabled after stop");
		tests::Expect(result, !persistenceRuntime.IsStarted(), "PersistenceRuntimeIntegration: stopped after stop");

		return result;
	}
}