#include "MatchHistoryPersistenceIntegrationTests.h"

#include <Windows.h>

#include <array>
#include <cstdint>
#include <expected>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Identity/IdentityTypes.h>
#include <Common/Threading/ThreadPool.h>
#include <Common/Time/TimeTypes.h>

#include <Persistence/Account/AccountRepository.h>
#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Core/PersistenceRuntime.h>
#include <Persistence/Match/MatchHistoryRepository.h>
#include <Persistence/Odbc/OdbcConnection.h>
#include <Persistence/Odbc/OdbcEnvironment.h>
#include <Persistence/Odbc/OdbcStatement.h>

#include <Server/Game/MatchHistoryTracker.h>
#include <Server/Match/MatchHistoryTaskProcessor.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using DatabaseError = persistence::core::DatabaseError;
	using PersistentPlayerId = common::identity::PersistentPlayerId;
	using MatchId = common::identity::MatchId;
	using RoomId = common::game::RoomId;

	using DatabaseOperationResult = std::expected<void, DatabaseError>;
	using CountResult = std::expected<std::int64_t, DatabaseError>;

	struct MatchPlayerRow
	{
	public:
		PersistentPlayerId persistentPlayerId = 0;
		std::int64_t killCount = 0;
		std::int64_t deathCount = 0;
	};

	using MatchPlayerRowList = std::vector<MatchPlayerRow>;
	using MatchPlayerRowListResult = std::expected<MatchPlayerRowList, DatabaseError>;

	[[nodiscard]] constexpr std::string_view GetDatabaseConnectionStringEnvironmentName() noexcept
	{
		return "WINDOWS_IOCP_UDP_TEST_DB_CONNECTION_STRING";
	}

	[[nodiscard]] std::optional<std::string> ReadDatabaseConnectionString()
	{
		const std::string_view environmentName = GetDatabaseConnectionStringEnvironmentName();

		const DWORD requiredSize = ::GetEnvironmentVariableA(environmentName.data(), nullptr, 0);
		if (requiredSize == 0)
		{
			return std::nullopt;
		}

		std::string value(requiredSize, '\0');

		const DWORD copiedSize = ::GetEnvironmentVariableA(
			environmentName.data(),
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

	void AddDatabaseFailure(tests::DebugTestResult& result, std::string_view operationName, const DatabaseError& error)
	{
		std::string message(operationName);
		message += ": ";
		message += persistence::core::ToString(error);

		result.AddFailed(message);
	}

	[[nodiscard]] DatabaseOperationResult DeleteAccountByLoginName(
		persistence::odbc::OdbcConnection& connection,
		std::string_view loginName
	)
	{
		persistence::odbc::OdbcStatement statement;

		const auto prepareResult = statement.Prepare(
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

		const auto bindResult = statement.BindInputString(
			static_cast<SQLUSMALLINT>(1),
			loginName
		);

		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const auto executeResult = statement.Execute();
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		return {};
	}

	[[nodiscard]] DatabaseOperationResult DeleteMatchesByRoom(
		persistence::odbc::OdbcConnection& connection,
		RoomId roomId
	)
	{
		persistence::odbc::OdbcStatement statement;

		const auto prepareResult = statement.Prepare(
			connection,
			R"sql(
DELETE FROM dbo.matches
WHERE room_id = ?;
)sql"
);

		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const auto bindResult = statement.BindInputInt64(
			static_cast<SQLUSMALLINT>(1),
			static_cast<std::int64_t>(roomId)
		);

		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const auto executeResult = statement.Execute();
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		return {};
	}

	[[nodiscard]] std::expected<PersistentPlayerId, DatabaseError> CreatePersistentPlayer(
		persistence::PersistenceRuntime& persistenceRuntime,
		std::string_view loginName,
		std::string_view nickname
	)
	{
		const auto createAccountResult = persistenceRuntime.CreateAccount(
			persistence::account::AccountCreateRequest{
				.loginName = loginName,
				.passwordHash = "match_history_test_hash",
				.nickname = nickname,
			}
			);

		if (!createAccountResult.has_value())
		{
			return std::unexpected(createAccountResult.error());
		}

		const auto playerResult = persistenceRuntime.FindOrCreatePlayerByAccountId(createAccountResult->accountId);
		if (!playerResult.has_value())
		{
			return std::unexpected(playerResult.error());
		}

		return playerResult->playerId;
	}

	[[nodiscard]] CountResult CountMatchesByRoom(
		persistence::odbc::OdbcConnection& connection,
		RoomId roomId
	)
	{
		persistence::odbc::OdbcStatement statement;

		const auto prepareResult = statement.Prepare(
			connection,
			R"sql(
SELECT COUNT_BIG(*)
FROM dbo.matches
WHERE room_id = ?;
)sql"
);

		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const auto bindResult = statement.BindInputInt64(
			static_cast<SQLUSMALLINT>(1),
			static_cast<std::int64_t>(roomId)
		);

		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const auto executeResult = statement.Execute();
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		const auto fetchResult = statement.Fetch();
		if (!fetchResult.has_value())
		{
			return std::unexpected(fetchResult.error());
		}

		if (!*fetchResult)
		{
			return std::unexpected(DatabaseError{
				.failure = persistence::core::DatabaseFailure::StatementFetchFailed,
				.message = "Match count query returned no row.",
				});
		}

		return statement.ReadInt64(static_cast<SQLUSMALLINT>(1));
	}

	[[nodiscard]] std::expected<RoomId, DatabaseError> ReadMatchRoomId(
		persistence::odbc::OdbcConnection& connection,
		MatchId matchId
	)
	{
		persistence::odbc::OdbcStatement statement;

		const auto prepareResult = statement.Prepare(
			connection,
			R"sql(
SELECT room_id
FROM dbo.matches
WHERE match_id = ?;
)sql"
);

		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const auto bindResult = statement.BindInputInt64(
			static_cast<SQLUSMALLINT>(1),
			matchId
		);

		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const auto executeResult = statement.Execute();
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		const auto fetchResult = statement.Fetch();
		if (!fetchResult.has_value())
		{
			return std::unexpected(fetchResult.error());
		}

		if (!*fetchResult)
		{
			return std::unexpected(DatabaseError{
				.failure = persistence::core::DatabaseFailure::StatementFetchFailed,
				.message = "Saved match row was not found.",
				});
		}

		const auto roomIdResult = statement.ReadInt32(static_cast<SQLUSMALLINT>(1));
		if (!roomIdResult.has_value())
		{
			return std::unexpected(roomIdResult.error());
		}

		return static_cast<RoomId>(*roomIdResult);
	}

	[[nodiscard]] MatchPlayerRowListResult ReadMatchPlayerRows(
		persistence::odbc::OdbcConnection& connection,
		MatchId matchId
	)
	{
		persistence::odbc::OdbcStatement statement;

		const auto prepareResult = statement.Prepare(
			connection,
			R"sql(
SELECT
	player_id,
	kill_count,
	death_count
FROM dbo.match_players
WHERE match_id = ?
ORDER BY player_id;
)sql"
);

		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const auto bindResult = statement.BindInputInt64(
			static_cast<SQLUSMALLINT>(1),
			matchId
		);

		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const auto executeResult = statement.Execute();
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		MatchPlayerRowList rowList;

		while (true)
		{
			const auto fetchResult = statement.Fetch();
			if (!fetchResult.has_value())
			{
				return std::unexpected(fetchResult.error());
			}

			if (!*fetchResult)
			{
				break;
			}

			const auto playerIdResult = statement.ReadInt64(static_cast<SQLUSMALLINT>(1));
			if (!playerIdResult.has_value())
			{
				return std::unexpected(playerIdResult.error());
			}

			const auto killCountResult = statement.ReadInt64(static_cast<SQLUSMALLINT>(2));
			if (!killCountResult.has_value())
			{
				return std::unexpected(killCountResult.error());
			}

			const auto deathCountResult = statement.ReadInt64(static_cast<SQLUSMALLINT>(3));
			if (!deathCountResult.has_value())
			{
				return std::unexpected(deathCountResult.error());
			}

			rowList.push_back(MatchPlayerRow{
				.persistentPlayerId = *playerIdResult,
				.killCount = *killCountResult,
				.deathCount = *deathCountResult,
				});
		}

		return rowList;
	}

	[[nodiscard]] const MatchPlayerRow* FindMatchPlayerRow(
		const MatchPlayerRowList& rowList,
		PersistentPlayerId persistentPlayerId
	) noexcept
	{
		for (const MatchPlayerRow& row : rowList)
		{
			if (row.persistentPlayerId == persistentPlayerId)
			{
				return &row;
			}
		}

		return nullptr;
	}

	void RunAsyncSaveTest(
		tests::DebugTestResult& result,
		persistence::PersistenceRuntime& persistenceRuntime,
		persistence::odbc::OdbcConnection& queryConnection
	)
	{
		constexpr RoomId roomId = 910001;
		constexpr std::string_view firstLoginName = "match_history_async_player_1";
		constexpr std::string_view secondLoginName = "match_history_async_player_2";

		static_cast<void>(DeleteMatchesByRoom(queryConnection, roomId));
		static_cast<void>(DeleteAccountByLoginName(queryConnection, firstLoginName));
		static_cast<void>(DeleteAccountByLoginName(queryConnection, secondLoginName));

		const auto firstPlayerResult = CreatePersistentPlayer(
			persistenceRuntime,
			firstLoginName,
			"MatchHistoryAsyncPlayer1"
		);

		if (!firstPlayerResult.has_value())
		{
			AddDatabaseFailure(result, "MatchHistoryPersistenceIntegration: first player creation failed", firstPlayerResult.error());
			return;
		}

		const auto secondPlayerResult = CreatePersistentPlayer(
			persistenceRuntime,
			secondLoginName,
			"MatchHistoryAsyncPlayer2"
		);

		if (!secondPlayerResult.has_value())
		{
			AddDatabaseFailure(result, "MatchHistoryPersistenceIntegration: second player creation failed", secondPlayerResult.error());
			return;
		}

		const PersistentPlayerId firstPlayerId = *firstPlayerResult;
		const PersistentPlayerId secondPlayerId = *secondPlayerResult;

		server::match::MatchHistoryTaskProcessor taskProcessor(persistenceRuntime);

		const auto startResult = taskProcessor.Start();
		if (!startResult.has_value())
		{
			std::string message = "MatchHistoryPersistenceIntegration: task processor start failed: ";
			message += common::threading::ThreadPool::ToString(startResult.error());
			result.AddFailed(message);
			return;
		}

		const common::time::SystemTimePoint startedAt = common::time::SystemClock::now();
		const common::time::SystemTimePoint endedAt = startedAt + common::time::Seconds(15);

		server::game::CompletedMatch completedMatch{
			.roomId = roomId,
			.startedAt = startedAt,
			.endedAt = endedAt,
			.playerStatsList = {
				server::game::MatchPlayerStats{
					.persistentPlayerId = firstPlayerId,
					.killCount = 2,
					.deathCount = 1,
				},
				server::game::MatchPlayerStats{
					.persistentPlayerId = secondPlayerId,
					.killCount = 1,
					.deathCount = 2,
				},
			},
		};

		const bool enqueued = taskProcessor.Enqueue(std::move(completedMatch));
		tests::Expect(result, enqueued, "MatchHistoryPersistenceIntegration: completed match enqueued");

		if (!enqueued)
		{
			taskProcessor.Stop();
			return;
		}

		taskProcessor.StopAfterDrain();

		server::match::MatchHistoryTaskProcessor::CompletionList completionList = taskProcessor.ExtractCompletionList();

		tests::Expect(result, completionList.size() == 1, "MatchHistoryPersistenceIntegration: one save completion");

		if (completionList.size() != 1)
		{
			return;
		}

		server::match::MatchHistorySaveCompletion& completion = completionList.front();

		if (!completion.saveResult.has_value())
		{
			AddDatabaseFailure(result, "MatchHistoryPersistenceIntegration: async save failed", completion.saveResult.error());
			return;
		}

		const MatchId matchId = *completion.saveResult;

		tests::Expect(result, matchId > 0, "MatchHistoryPersistenceIntegration: valid match id returned");
		tests::Expect(result, completion.roomId == roomId, "MatchHistoryPersistenceIntegration: completion room id");

		const auto roomIdResult = ReadMatchRoomId(queryConnection, matchId);
		if (!roomIdResult.has_value())
		{
			AddDatabaseFailure(result, "MatchHistoryPersistenceIntegration: saved match lookup failed", roomIdResult.error());
			return;
		}

		tests::Expect(result, *roomIdResult == roomId, "MatchHistoryPersistenceIntegration: persisted room id");

		const MatchPlayerRowListResult rowListResult = ReadMatchPlayerRows(queryConnection, matchId);
		if (!rowListResult.has_value())
		{
			AddDatabaseFailure(result, "MatchHistoryPersistenceIntegration: match player lookup failed", rowListResult.error());
			return;
		}

		tests::Expect(result, rowListResult->size() == 2, "MatchHistoryPersistenceIntegration: two match players persisted");

		const MatchPlayerRow* firstRow = FindMatchPlayerRow(*rowListResult, firstPlayerId);
		const MatchPlayerRow* secondRow = FindMatchPlayerRow(*rowListResult, secondPlayerId);

		tests::Expect(result, firstRow != nullptr, "MatchHistoryPersistenceIntegration: first player row exists");
		tests::Expect(result, secondRow != nullptr, "MatchHistoryPersistenceIntegration: second player row exists");

		if (firstRow != nullptr)
		{
			tests::Expect(result, firstRow->killCount == 2, "MatchHistoryPersistenceIntegration: first player kill count");
			tests::Expect(result, firstRow->deathCount == 1, "MatchHistoryPersistenceIntegration: first player death count");
		}

		if (secondRow != nullptr)
		{
			tests::Expect(result, secondRow->killCount == 1, "MatchHistoryPersistenceIntegration: second player kill count");
			tests::Expect(result, secondRow->deathCount == 2, "MatchHistoryPersistenceIntegration: second player death count");
		}

		static_cast<void>(DeleteMatchesByRoom(queryConnection, roomId));
		static_cast<void>(DeleteAccountByLoginName(queryConnection, firstLoginName));
		static_cast<void>(DeleteAccountByLoginName(queryConnection, secondLoginName));
	}

	void RunRollbackTest(
		tests::DebugTestResult& result,
		persistence::PersistenceRuntime& persistenceRuntime,
		persistence::odbc::OdbcConnection& queryConnection
	)
	{
		constexpr RoomId roomId = 910002;
		constexpr std::string_view loginName = "match_history_rollback_player";

		static_cast<void>(DeleteMatchesByRoom(queryConnection, roomId));
		static_cast<void>(DeleteAccountByLoginName(queryConnection, loginName));

		const auto playerResult = CreatePersistentPlayer(
			persistenceRuntime,
			loginName,
			"MatchHistoryRollbackPlayer"
		);

		if (!playerResult.has_value())
		{
			AddDatabaseFailure(result, "MatchHistoryPersistenceIntegration: rollback player creation failed", playerResult.error());
			return;
		}

		const common::time::SystemTimePoint startedAt = common::time::SystemClock::now();
		const common::time::SystemTimePoint endedAt = startedAt + common::time::Seconds(5);

		const std::array<persistence::match::MatchPlayerCreateRecord, 2> playerStatsList{
			persistence::match::MatchPlayerCreateRecord{
				.persistentPlayerId = *playerResult,
				.killCount = 1,
				.deathCount = 0,
			},
			persistence::match::MatchPlayerCreateRecord{
				.persistentPlayerId = 0,
				.killCount = 0,
				.deathCount = 1,
			},
		};

		const auto saveResult = persistenceRuntime.SaveMatch(
			persistence::match::MatchCreateRequest{
				.roomId = roomId,
				.startedAt = startedAt,
				.endedAt = endedAt,
				.playerStatsList = playerStatsList,
			}
			);

		tests::Expect(result, !saveResult.has_value(), "MatchHistoryPersistenceIntegration: invalid player causes save failure");

		const CountResult countResult = CountMatchesByRoom(queryConnection, roomId);
		if (!countResult.has_value())
		{
			AddDatabaseFailure(result, "MatchHistoryPersistenceIntegration: rollback verification failed", countResult.error());
			return;
		}

		tests::Expect(result, *countResult == 0, "MatchHistoryPersistenceIntegration: failed save rolls back match row");

		static_cast<void>(DeleteMatchesByRoom(queryConnection, roomId));
		static_cast<void>(DeleteAccountByLoginName(queryConnection, loginName));
	}
}

namespace tests::server
{
	DebugTestResult RunMatchHistoryPersistenceIntegrationTests()
	{
		DebugTestResult result{};

		const std::optional<std::string> connectionString = ReadDatabaseConnectionString();
		if (!connectionString.has_value())
		{
			std::cout << "[MatchHistoryPersistenceIntegration] Skipped: "
				<< GetDatabaseConnectionStringEnvironmentName()
				<< " is not set.\n";

			return result;
		}

		persistence::PersistenceRuntime persistenceRuntime;

		const auto persistenceStartResult = persistenceRuntime.Start(
			persistence::PersistenceRuntimeStartConfig{
				.enabled = true,
				.connectionString = *connectionString,
				.connectionTimeoutSeconds = 5,
			}
			);

		if (!persistenceStartResult.has_value())
		{
			AddDatabaseFailure(result, "MatchHistoryPersistenceIntegration: persistence start failed", persistenceStartResult.error());
			return result;
		}

		persistence::odbc::OdbcEnvironment queryEnvironment;

		const auto initializeResult = queryEnvironment.Initialize();
		if (!initializeResult.has_value())
		{
			AddDatabaseFailure(result, "MatchHistoryPersistenceIntegration: query environment initialization failed", initializeResult.error());
			persistenceRuntime.Stop();
			return result;
		}

		persistence::odbc::OdbcConnection queryConnection;

		const auto openResult = queryConnection.Open(
			queryEnvironment,
			persistence::odbc::OdbcConnectionOpenConfig{
				.connectionString = *connectionString,
				.connectionTimeoutSeconds = 5,
			}
			);

		if (!openResult.has_value())
		{
			AddDatabaseFailure(result, "MatchHistoryPersistenceIntegration: query connection failed", openResult.error());
			persistenceRuntime.Stop();
			return result;
		}

		RunAsyncSaveTest(result, persistenceRuntime, queryConnection);
		RunRollbackTest(result, persistenceRuntime, queryConnection);

		queryConnection.Close();
		queryEnvironment.Close();
		persistenceRuntime.Stop();

		return result;
	}
}