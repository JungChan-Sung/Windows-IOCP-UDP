#include "MatchHistoryRepository.h"

#include <cstdint>
#include <string>
#include <utility>

#include <Persistence/Odbc/OdbcStatement.h>

namespace
{
	[[nodiscard]] persistence::core::DatabaseError MakeConnectionNotOpenError()
	{
		return persistence::core::DatabaseError{
			.failure = persistence::core::DatabaseFailure::ConnectionOpenFailed,
			.message = "Database connection is not open.",
		};
	}
}

namespace persistence::match
{
	MatchHistoryRepository::MatchHistoryRepository(odbc::OdbcConnection& connection) noexcept
		: connection_(connection)
	{}

	MatchHistoryRepository::SaveMatchResult MatchHistoryRepository::SaveMatch(const MatchCreateRequest& request)
	{
		if (!connection_.IsOpen())
		{
			return std::unexpected(MakeConnectionNotOpenError());
		}

		// Match 본문과 모든 참가자 통계를 하나의 작업 단위로 저장해 부분 저장을 방지
		const odbc::OdbcConnection::TransactionResult beginResult = connection_.BeginTransaction();
		if (!beginResult.has_value())
		{
			return std::unexpected(beginResult.error());
		}

		// Transaction 중간 실패 시 전체 변경을 되돌리고,
		// Rollback 실패도 원래 오류 정보에 함께 보존
		const auto rollbackWithError = [this](core::DatabaseError error) -> SaveMatchResult
			{
				if (connection_.IsTransactionActive())
				{
					const odbc::OdbcConnection::TransactionResult rollbackResult = connection_.RollbackTransaction();
					if (!rollbackResult.has_value())
					{
						error.message += " Rollback failed: ";
						error.message += core::ToString(rollbackResult.error());
					}
				}

				return std::unexpected(std::move(error));
			};

		odbc::OdbcStatement matchStatement;
		const odbc::OdbcStatement::ExecuteResult matchPrepareResult = matchStatement.Prepare(
			connection_,
			R"sql(
INSERT INTO dbo.matches
(
	room_id,
	started_at_utc,
	ended_at_utc
)
OUTPUT
	INSERTED.match_id
VALUES
(
	?,
	?,
	?
);
)sql"
);
		if (!matchPrepareResult.has_value())
		{
			return rollbackWithError(matchPrepareResult.error());
		}

		const odbc::OdbcStatement::BindResult roomIdBindResult =
			matchStatement.BindInputInt64(static_cast<SQLUSMALLINT>(1), static_cast<std::int64_t>(request.roomId));
		if (!roomIdBindResult.has_value())
		{
			return rollbackWithError(roomIdBindResult.error());
		}

		const odbc::OdbcStatement::BindResult startedAtBindResult = matchStatement.BindInputSystemTimePoint(static_cast<SQLUSMALLINT>(2), request.startedAt);
		if (!startedAtBindResult.has_value())
		{
			return rollbackWithError(startedAtBindResult.error());
		}

		const odbc::OdbcStatement::BindResult endedAtBindResult = matchStatement.BindInputSystemTimePoint(static_cast<SQLUSMALLINT>(3), request.endedAt);
		if (!endedAtBindResult.has_value())
		{
			return rollbackWithError(endedAtBindResult.error());
		}

		const odbc::OdbcStatement::ExecuteResult matchExecuteResult = matchStatement.Execute();
		if (!matchExecuteResult.has_value())
		{
			return rollbackWithError(matchExecuteResult.error());
		}

		const odbc::OdbcStatement::FetchResult matchFetchResult = matchStatement.Fetch();
		if (!matchFetchResult.has_value())
		{
			return rollbackWithError(matchFetchResult.error());
		}

		if (!*matchFetchResult)
		{
			return rollbackWithError(core::DatabaseError{
				.failure = core::DatabaseFailure::StatementFetchFailed,
				.message = "Created match record was not returned.",
				});
		}

		const odbc::OdbcStatement::ReadInt64Result matchIdResult = matchStatement.ReadInt64(static_cast<SQLUSMALLINT>(1));
		if (!matchIdResult.has_value())
		{
			return rollbackWithError(matchIdResult.error());
		}

		const MatchId matchId = *matchIdResult;

		for (const MatchPlayerCreateRecord& playerStats : request.playerStatsList)
		{
			odbc::OdbcStatement playerStatement;

			const odbc::OdbcStatement::ExecuteResult playerPrepareResult = playerStatement.Prepare(
				connection_,
				R"sql(
INSERT INTO dbo.match_players
(
	match_id,
	player_id,
	kill_count,
	death_count
)
VALUES
(
	?,
	?,
	?,
	?
);
)sql"
);
			if (!playerPrepareResult.has_value())
			{
				return rollbackWithError(playerPrepareResult.error());
			}

			const odbc::OdbcStatement::BindResult matchIdBindResult = playerStatement.BindInputInt64(static_cast<SQLUSMALLINT>(1), matchId);
			if (!matchIdBindResult.has_value())
			{
				return rollbackWithError(matchIdBindResult.error());
			}

			const odbc::OdbcStatement::BindResult playerIdBindResult = playerStatement.BindInputInt64(static_cast<SQLUSMALLINT>(2), playerStats.persistentPlayerId);
			if (!playerIdBindResult.has_value())
			{
				return rollbackWithError(playerIdBindResult.error());
			}

			const odbc::OdbcStatement::BindResult killCountBindResult =
				playerStatement.BindInputInt64(static_cast<SQLUSMALLINT>(3), static_cast<std::int64_t>(playerStats.killCount));
			if (!killCountBindResult.has_value())
			{
				return rollbackWithError(killCountBindResult.error());
			}

			const odbc::OdbcStatement::BindResult deathCountBindResult =
				playerStatement.BindInputInt64(static_cast<SQLUSMALLINT>(4), static_cast<std::int64_t>(playerStats.deathCount));
			if (!deathCountBindResult.has_value())
			{
				return rollbackWithError(deathCountBindResult.error());
			}

			const odbc::OdbcStatement::ExecuteResult playerExecuteResult = playerStatement.Execute();
			if (!playerExecuteResult.has_value())
			{
				return rollbackWithError(playerExecuteResult.error());
			}
		}

		const odbc::OdbcConnection::TransactionResult commitResult = connection_.CommitTransaction();
		if (!commitResult.has_value())
		{
			return rollbackWithError(commitResult.error());
		}

		return matchId;
	}
}