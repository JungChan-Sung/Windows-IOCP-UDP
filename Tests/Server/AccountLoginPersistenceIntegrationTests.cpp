#include "AccountLoginPersistenceIntegrationTests.h"

#include <Windows.h>

#include <cstdint>
#include <expected>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <Common/Net/EndpointKey.h>
#include <Common/Net/SessionToken.h>
#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Time/TimeTypes.h>

#include <Persistence/Account/AccountRepository.h>
#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Core/PersistenceRuntime.h>
#include <Persistence/Odbc/OdbcConnection.h>
#include <Persistence/Odbc/OdbcEnvironment.h>
#include <Persistence/Odbc/OdbcStatement.h>

#include <Server/Account/AccountLoginTaskProcessor.h>
#include <Server/Account/AccountService.h>
#include <Server/Protocol/AccountLoginPacketHandler.h>
#include <Server/Protocol/AccountPacketMapper.h>
#include <Server/Service/AccountLoginAdmissionService.h>
#include <Server/Service/AuthenticatedAccountRegistry.h>
#include <Server/Service/PeerRoomManager.h>

#include <Tests/DebugTestResult.h>

namespace
{
	inline constexpr std::string_view databaseConnectionStringEnvironmentName =
		"WINDOWS_IOCP_UDP_TEST_DB_CONNECTION_STRING";

	inline constexpr std::string_view testLoginName = "account_login_persistence_integration_test";
	inline constexpr std::string_view testPasswordHash = "account_login_persistence_hash";
	inline constexpr std::string_view testNickname = "PersistenceLogin";

	using DeleteAccountResult = std::expected<void, persistence::core::DatabaseError>;

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

	[[nodiscard]] constexpr common::net::EndpointKey MakeEndpointKey() noexcept
	{
		return common::net::EndpointKey{
			.address = 0x7F000001,
			.port = 45000,
		};
	}

	void AddDatabaseFailure(
		tests::DebugTestResult& result,
		std::string_view operationName,
		const persistence::core::DatabaseError& databaseError
	)
	{
		std::string message(operationName);
		message += ": ";
		message += persistence::core::ToString(databaseError);

		result.AddFailed(message);
	}

	[[nodiscard]] DeleteAccountResult DeleteAccountByLoginName(
		persistence::odbc::OdbcConnection& connection,
		std::string_view loginName
	)
	{
		persistence::odbc::OdbcStatement statement;

		const persistence::odbc::OdbcStatement::ExecuteResult prepareResult =
			statement.Prepare(
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

		const persistence::odbc::OdbcStatement::BindResult bindResult =
			statement.BindInputString(1, loginName);

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

	void RunLoginPersistenceFlowTest(
		tests::DebugTestResult& result,
		persistence::PersistenceRuntime& persistenceRuntime,
		persistence::odbc::OdbcConnection& cleanupConnection
	)
	{
		const DeleteAccountResult initialDeleteResult =
			DeleteAccountByLoginName(cleanupConnection, testLoginName);

		if (!initialDeleteResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountLoginPersistenceIntegration: initial cleanup",
				initialDeleteResult.error()
			);

			return;
		}

		const persistence::PersistenceRuntime::CreateAccountResult createAccountResult =
			persistenceRuntime.CreateAccount(
				persistence::account::AccountCreateRequest{
					.loginName = testLoginName,
					.passwordHash = testPasswordHash,
					.nickname = testNickname,
				}
				);

		if (!createAccountResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountLoginPersistenceIntegration: account creation",
				createAccountResult.error()
			);

			return;
		}

		const std::int64_t accountId = createAccountResult->accountId;

		const persistence::PersistenceRuntime::FindPlayerResult playerBeforeLoginResult =
			persistenceRuntime.FindPlayerByAccountId(accountId);

		if (!playerBeforeLoginResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountLoginPersistenceIntegration: player lookup before login",
				playerBeforeLoginResult.error()
			);

			return;
		}

		tests::Expect(
			result,
			!playerBeforeLoginResult->has_value(),
			"AccountLoginPersistenceIntegration: player missing before first login"
		);

		server::account::AccountService accountService(persistenceRuntime);
		server::account::AccountLoginTaskProcessor taskProcessor(accountService);
		server::protocol::AccountLoginPacketHandler packetHandler(taskProcessor);

		const server::account::AccountLoginTaskProcessor::StartResult processorStartResult = taskProcessor.Start(1);

		tests::Expect(
			result,
			processorStartResult.has_value(),
			"AccountLoginPersistenceIntegration: task processor starts"
		);

		if (!processorStartResult.has_value())
		{
			static_cast<void>(DeleteAccountByLoginName(cleanupConnection, testLoginName));
			return;
		}

		const common::net::EndpointKey endpointKey = MakeEndpointKey();
		const common::time::TimePoint requestTime = common::time::Clock::now();

		common::packet::AccountLoginRequestPacket requestPacket{};
		requestPacket.requestId = 3001;
		requestPacket.loginName = testLoginName;
		requestPacket.passwordHash = testPasswordHash;

		const server::protocol::AccountLoginPacketHandler::EnqueueStatus enqueueStatus =
			packetHandler.Enqueue(endpointKey, requestPacket, requestTime);

		tests::Expect(
			result,
			enqueueStatus == server::protocol::AccountLoginPacketHandler::EnqueueStatus::Enqueued,
			"AccountLoginPersistenceIntegration: login request enqueued"
		);

		taskProcessor.StopAfterDrain();

		if (enqueueStatus != server::protocol::AccountLoginPacketHandler::EnqueueStatus::Enqueued)
		{
			static_cast<void>(DeleteAccountByLoginName(cleanupConnection, testLoginName));
			return;
		}

		const common::time::TimePoint completionTime = requestTime + common::time::Seconds(1);

		server::protocol::AccountLoginPacketHandler::ResponseTaskList responseTaskList =
			packetHandler.ExtractResponseTaskList(completionTime);

		tests::Expect(
			result,
			responseTaskList.size() == 1,
			"AccountLoginPersistenceIntegration: one login response extracted"
		);

		if (responseTaskList.size() == 1)
		{
			server::protocol::AccountLoginPacketHandler::ResponseTask& responseTask =
				responseTaskList.front();

			tests::Expect(
				result,
				responseTask.taskId != server::protocol::AccountLoginPacketHandler::invalidTaskId,
				"AccountLoginPersistenceIntegration: response has task id"
			);

			tests::Expect(
				result,
				responseTask.isLatestRequest,
				"AccountLoginPersistenceIntegration: response is latest request"
			);

			tests::Expect(
				result,
				responseTask.endpointKey == endpointKey,
				"AccountLoginPersistenceIntegration: response endpoint preserved"
			);

			tests::Expect(
				result,
				responseTask.responsePacket.status == common::packet::AccountLoginResponseStatus::Succeeded,
				"AccountLoginPersistenceIntegration: account login succeeds"
			);

			tests::Expect(
				result,
				responseTask.responsePacket.accountId == accountId,
				"AccountLoginPersistenceIntegration: response account id matches"
			);

			tests::Expect(
				result,
				responseTask.responsePacket.sessionToken == common::net::invalidSessionToken,
				"AccountLoginPersistenceIntegration: mapper does not issue session token"
			);

			tests::Expect(
				result,
				responseTask.responsePacket.nickname == testNickname,
				"AccountLoginPersistenceIntegration: response nickname matches"
			);

			tests::Expect(
				result,
				responseTask.persistentPlayerId > 0,
				"AccountLoginPersistenceIntegration: persistent player id propagated"
			);

			const persistence::PersistenceRuntime::FindPlayerResult playerResult =
				persistenceRuntime.FindPlayerByAccountId(accountId);

			if (!playerResult.has_value())
			{
				AddDatabaseFailure(
					result,
					"AccountLoginPersistenceIntegration: persistent player lookup",
					playerResult.error()
				);
			}
			else
			{
				tests::Expect(
					result,
					playerResult->has_value(),
					"AccountLoginPersistenceIntegration: persistent player created"
				);

				if (playerResult->has_value())
				{
					tests::Expect(
						result,
						(**playerResult).playerId == responseTask.persistentPlayerId,
						"AccountLoginPersistenceIntegration: response task player id matches database"
					);

					tests::Expect(
						result,
						(**playerResult).accountId == accountId,
						"AccountLoginPersistenceIntegration: persistent player account id matches"
					);
				}
			}

			server::service::AccountLoginAdmissionService admissionService;
			server::service::AuthenticatedAccountRegistry authenticatedAccountRegistry;
			server::service::PeerRoomManager peerRoomManager;

			const server::service::AccountLoginAdmissionService::Request admissionRequest{
				.endpointKey = endpointKey,
				.accountId = responseTask.responsePacket.accountId,
				.persistentPlayerId = responseTask.persistentPlayerId,
				.nickname = responseTask.responsePacket.nickname,
				.currentTime = completionTime,
			};

			const server::service::AccountLoginAdmissionService::Result admissionResult =
				admissionService.Apply(
					admissionRequest,
					authenticatedAccountRegistry,
					peerRoomManager
				);

			server::protocol::ApplyAccountLoginAdmissionResult(
				admissionResult,
				responseTask.responsePacket
			);

			tests::Expect(
				result,
				admissionResult.status == server::service::AccountLoginAdmissionService::Status::Authenticated,
				"AccountLoginPersistenceIntegration: login admitted"
			);

			tests::Expect(
				result,
				common::net::IsValidSessionToken(responseTask.responsePacket.sessionToken),
				"AccountLoginPersistenceIntegration: session token issued after admission"
			);

			const server::service::AuthenticatedAccount* authenticatedAccount =
				authenticatedAccountRegistry.Find(endpointKey);

			tests::Expect(
				result,
				authenticatedAccount != nullptr,
				"AccountLoginPersistenceIntegration: authenticated account registered"
			);

			if (authenticatedAccount != nullptr)
			{
				tests::Expect(
					result,
					authenticatedAccount->accountId == accountId,
					"AccountLoginPersistenceIntegration: registry account id matches"
				);

				tests::Expect(
					result,
					authenticatedAccount->persistentPlayerId == responseTask.persistentPlayerId,
					"AccountLoginPersistenceIntegration: registry persistent player id matches"
				);

				tests::Expect(
					result,
					authenticatedAccount->sessionToken == responseTask.responsePacket.sessionToken,
					"AccountLoginPersistenceIntegration: registry session token matches response"
				);
			}

			const bool finalized =
				packetHandler.FinalizeResponse(
					responseTask.taskId,
					responseTask.responsePacket,
					completionTime
				);

			tests::Expect(
				result,
				finalized,
				"AccountLoginPersistenceIntegration: response finalized"
			);

			if (finalized)
			{
				const server::protocol::AccountLoginPacketHandler::EnqueueStatus cachedEnqueueStatus =
					packetHandler.Enqueue(
						endpointKey,
						requestPacket,
						completionTime + common::time::Milliseconds(1)
					);

				tests::Expect(
					result,
					cachedEnqueueStatus == server::protocol::AccountLoginPacketHandler::EnqueueStatus::CachedResponseQueued,
					"AccountLoginPersistenceIntegration: duplicate request uses cached response"
				);

				server::protocol::AccountLoginPacketHandler::ResponseTaskList cachedResponseTaskList =
					packetHandler.ExtractResponseTaskList(
						completionTime + common::time::Milliseconds(1)
					);

				tests::Expect(
					result,
					cachedResponseTaskList.size() == 1,
					"AccountLoginPersistenceIntegration: cached response extracted"
				);

				if (cachedResponseTaskList.size() == 1)
				{
					const server::protocol::AccountLoginPacketHandler::ResponseTask& cachedResponseTask =
						cachedResponseTaskList.front();

					tests::Expect(
						result,
						cachedResponseTask.taskId == server::protocol::AccountLoginPacketHandler::invalidTaskId,
						"AccountLoginPersistenceIntegration: cached response has no task id"
					);

					tests::Expect(
						result,
						cachedResponseTask.endpointKey == endpointKey,
						"AccountLoginPersistenceIntegration: cached response endpoint preserved"
					);

					tests::Expect(
						result,
						cachedResponseTask.persistentPlayerId == 0,
						"AccountLoginPersistenceIntegration: cached response does not repeat server-only player id"
					);

					tests::Expect(
						result,
						cachedResponseTask.responsePacket.sessionToken == responseTask.responsePacket.sessionToken,
						"AccountLoginPersistenceIntegration: cached response preserves session token"
					);
				}
			}
		}

		const DeleteAccountResult finalDeleteResult =
			DeleteAccountByLoginName(cleanupConnection, testLoginName);

		if (!finalDeleteResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountLoginPersistenceIntegration: final cleanup",
				finalDeleteResult.error()
			);

			return;
		}

		const persistence::PersistenceRuntime::FindPlayerResult playerAfterDeleteResult =
			persistenceRuntime.FindPlayerByAccountId(accountId);

		if (!playerAfterDeleteResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountLoginPersistenceIntegration: player lookup after account deletion",
				playerAfterDeleteResult.error()
			);

			return;
		}

		tests::Expect(
			result,
			!playerAfterDeleteResult->has_value(),
			"AccountLoginPersistenceIntegration: account deletion cascades persistent player"
		);
	}
}

namespace tests::server
{
	DebugTestResult RunAccountLoginPersistenceIntegrationTests()
	{
		DebugTestResult result{};

		const std::optional<std::string> connectionString = ReadDatabaseConnectionString();

		if (!connectionString.has_value())
		{
			std::cout
				<< "[AccountLoginPersistenceIntegration] Skipped: "
				<< databaseConnectionStringEnvironmentName
				<< " is not set.\n";

			return result;
		}

		persistence::PersistenceRuntime persistenceRuntime;

		const persistence::PersistenceRuntime::StartResult startResult =
			persistenceRuntime.Start(
				persistence::PersistenceRuntimeStartConfig{
					.enabled = true,
					.connectionString = *connectionString,
					.connectionTimeoutSeconds = 5,
				}
				);

		if (!startResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountLoginPersistenceIntegration: persistence runtime start",
				startResult.error()
			);

			return result;
		}

		persistence::odbc::OdbcEnvironment cleanupEnvironment;

		const persistence::odbc::OdbcEnvironment::InitializeResult initializeResult =
			cleanupEnvironment.Initialize();

		if (!initializeResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountLoginPersistenceIntegration: cleanup environment initialization",
				initializeResult.error()
			);

			persistenceRuntime.Stop();
			return result;
		}

		persistence::odbc::OdbcConnection cleanupConnection;

		const persistence::odbc::OdbcConnection::OpenResult openResult =
			cleanupConnection.Open(
				cleanupEnvironment,
				persistence::odbc::OdbcConnectionOpenConfig{
					.connectionString = *connectionString,
					.connectionTimeoutSeconds = 5,
				}
				);

		if (!openResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountLoginPersistenceIntegration: cleanup connection",
				openResult.error()
			);

			persistenceRuntime.Stop();
			return result;
		}

		RunLoginPersistenceFlowTest(
			result,
			persistenceRuntime,
			cleanupConnection
		);

		persistenceRuntime.Stop();

		return result;
	}
}