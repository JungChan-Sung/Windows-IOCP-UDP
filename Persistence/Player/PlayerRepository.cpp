#include "PlayerRepository.h"

#include <Common/Identity/IdentityTypes.h>

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

namespace persistence::player
{

	PlayerRepository::PlayerRepository(odbc::OdbcConnection& connection) noexcept
		: connection_(connection)
	{}

	PlayerRepository::CreatePlayerResult PlayerRepository::CreatePlayer(common::identity::AccountId accountId)
	{
		if (!connection_.IsOpen())
		{
			return std::unexpected(MakeConnectionNotOpenError());
		}

		odbc::OdbcStatement statement;
		const odbc::OdbcStatement::ExecuteResult prepareResult = statement.Prepare(
			connection_,
			R"sql(
INSERT INTO dbo.players
(
	account_id
)
OUTPUT
	INSERTED.player_id,
	INSERTED.account_id
VALUES
(
	?
);
)sql"
);
		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const odbc::OdbcStatement::BindResult bindResult = statement.BindInputInt64(static_cast<SQLUSMALLINT>(1), accountId);
		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const odbc::OdbcStatement::ExecuteResult executeResult = statement.Execute();
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		const odbc::OdbcStatement::FetchResult fetchResult = statement.Fetch();
		if (!fetchResult.has_value())
		{
			return std::unexpected(fetchResult.error());
		}

		if (!*fetchResult)
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::StatementFetchFailed,
				.message = "Created player record was not returned.",
				});
		}

		const odbc::OdbcStatement::ReadInt64Result playerIdResult = statement.ReadInt64(static_cast<SQLUSMALLINT>(1));
		if (!playerIdResult.has_value())
		{
			return std::unexpected(playerIdResult.error());
		}

		const odbc::OdbcStatement::ReadInt64Result accountIdResult = statement.ReadInt64(static_cast<SQLUSMALLINT>(2));
		if (!accountIdResult.has_value())
		{
			return std::unexpected(accountIdResult.error());
		}

		return PlayerRecord{
			.playerId = *playerIdResult,
			.accountId = *accountIdResult,
		};
	}

	PlayerRepository::FindPlayerResult PlayerRepository::FindPlayerByAccountId(common::identity::AccountId accountId)
	{
		if (!connection_.IsOpen())
		{
			return std::unexpected(MakeConnectionNotOpenError());
		}

		odbc::OdbcStatement statement;
		const odbc::OdbcStatement::ExecuteResult prepareResult = statement.Prepare(
			connection_,
			R"sql(
SELECT
	player_id,
	account_id
FROM dbo.players
WHERE account_id = ?;
)sql"
);
		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const odbc::OdbcStatement::BindResult bindResult = statement.BindInputInt64(static_cast<SQLUSMALLINT>(1), accountId);
		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const odbc::OdbcStatement::ExecuteResult executeResult = statement.Execute();
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		const odbc::OdbcStatement::FetchResult fetchResult = statement.Fetch();
		if (!fetchResult.has_value())
		{
			return std::unexpected(fetchResult.error());
		}

		if (!*fetchResult)
		{
			return std::nullopt;
		}

		const odbc::OdbcStatement::ReadInt64Result playerIdResult = statement.ReadInt64(static_cast<SQLUSMALLINT>(1));
		if (!playerIdResult.has_value())
		{
			return std::unexpected(playerIdResult.error());
		}

		const odbc::OdbcStatement::ReadInt64Result accountIdResult = statement.ReadInt64(static_cast<SQLUSMALLINT>(2));
		if (!accountIdResult.has_value())
		{
			return std::unexpected(accountIdResult.error());
		}

		return PlayerRecord{
			.playerId = *playerIdResult,
			.accountId = *accountIdResult,
		};
	}
}