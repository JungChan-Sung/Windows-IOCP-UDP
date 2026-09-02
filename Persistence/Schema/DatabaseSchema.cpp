#include "DatabaseSchema.h"

#include <format>
#include <string>
#include <string_view>

#include <Persistence/Account/AccountConstraints.h>
#include <Persistence/Config/ServerConfigConstraints.h>
#include <Persistence/Odbc/OdbcStatement.h>

namespace persistence::schema
{
	DatabaseSchema::InitializeResult DatabaseSchema::Initialize(odbc::OdbcConnection& connection)
	{
		const std::string createAccountsTableQuery = std::format(
			R"sql(
IF OBJECT_ID(N'dbo.accounts', N'U') IS NULL
BEGIN
	CREATE TABLE dbo.accounts
	(
		account_id BIGINT IDENTITY(1,1) NOT NULL,
		login_name NVARCHAR({}) NOT NULL,
		password_hash NVARCHAR({}) NOT NULL,
		nickname NVARCHAR({}) NOT NULL,
		created_at_utc DATETIME2(3) NOT NULL
			CONSTRAINT DF_accounts_created_at_utc
			DEFAULT SYSUTCDATETIME(),

		CONSTRAINT PK_accounts
			PRIMARY KEY (account_id),

		CONSTRAINT UQ_accounts_login_name
			UNIQUE (login_name)
	);
END
)sql",
account::maxLoginNameUtf16CodeUnitCount,
account::maxPasswordHashUtf16CodeUnitCount,
account::maxNicknameUtf16CodeUnitCount
);

		odbc::OdbcStatement statement;

		const odbc::OdbcStatement::ExecuteResult executeResult = statement.ExecuteDirect(connection, createAccountsTableQuery);
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		constexpr std::string_view createPlayersTableQuery = R"sql(
IF OBJECT_ID(N'dbo.players', N'U') IS NULL
BEGIN
	CREATE TABLE dbo.players
	(
		player_id BIGINT IDENTITY(1,1) NOT NULL,
		account_id BIGINT NOT NULL,
		created_at_utc DATETIME2(3) NOT NULL
			CONSTRAINT DF_players_created_at_utc
			DEFAULT SYSUTCDATETIME(),

		CONSTRAINT PK_players
			PRIMARY KEY (player_id),

		CONSTRAINT UQ_players_account_id
			UNIQUE (account_id),

		CONSTRAINT FK_players_accounts
			FOREIGN KEY (account_id)
			REFERENCES dbo.accounts(account_id)
			ON DELETE CASCADE
	);
END
)sql";

		odbc::OdbcStatement playerStatement;
		const odbc::OdbcStatement::ExecuteResult createPlayersResult = playerStatement.ExecuteDirect(connection, createPlayersTableQuery);
		if (!createPlayersResult.has_value())
		{
			return std::unexpected(createPlayersResult.error());
		}

		constexpr std::string_view createMatchesTableQuery = R"sql(
IF OBJECT_ID(N'dbo.matches', N'U') IS NULL
BEGIN
	CREATE TABLE dbo.matches
	(
		match_id BIGINT IDENTITY(1,1) NOT NULL,
		room_id INT NOT NULL,
		started_at_utc DATETIME2(3) NOT NULL,
		ended_at_utc DATETIME2(3) NOT NULL,

		CONSTRAINT PK_matches
			PRIMARY KEY (match_id),

		CONSTRAINT CK_matches_room_id
			CHECK (room_id > 0),

		CONSTRAINT CK_matches_time_range
			CHECK (ended_at_utc >= started_at_utc)
	);
END
)sql";

		odbc::OdbcStatement matchStatement;
		const odbc::OdbcStatement::ExecuteResult createMatchesResult = matchStatement.ExecuteDirect(connection, createMatchesTableQuery);
		if (!createMatchesResult.has_value())
		{
			return std::unexpected(createMatchesResult.error());
		}

		constexpr std::string_view createMatchPlayersTableQuery = R"sql(
IF OBJECT_ID(N'dbo.match_players', N'U') IS NULL
BEGIN
	CREATE TABLE dbo.match_players
	(
		match_id BIGINT NOT NULL,
		player_id BIGINT NOT NULL,
		kill_count BIGINT NOT NULL,
		death_count BIGINT NOT NULL,

		CONSTRAINT PK_match_players
			PRIMARY KEY (match_id, player_id),

		CONSTRAINT FK_match_players_matches
			FOREIGN KEY (match_id)
			REFERENCES dbo.matches(match_id)
			ON DELETE CASCADE,

		CONSTRAINT FK_match_players_players
			FOREIGN KEY (player_id)
			REFERENCES dbo.players(player_id)
			ON DELETE CASCADE,

		CONSTRAINT CK_match_players_kill_count
			CHECK (kill_count >= 0),

		CONSTRAINT CK_match_players_death_count
			CHECK (death_count >= 0)
	);

	CREATE INDEX IX_match_players_player_id
		ON dbo.match_players(player_id, match_id);
END
)sql";

		odbc::OdbcStatement matchPlayerStatement;
		const odbc::OdbcStatement::ExecuteResult createMatchPlayersResult = matchPlayerStatement.ExecuteDirect(connection, createMatchPlayersTableQuery);
		if (!createMatchPlayersResult.has_value())
		{
			return std::unexpected(createMatchPlayersResult.error());
		}

		const std::string createServerConfigsTableQuery = std::format(
			R"sql(
IF OBJECT_ID(N'dbo.server_configs', N'U') IS NULL
BEGIN
	CREATE TABLE dbo.server_configs
	(
		config_key NVARCHAR({}) NOT NULL,
		config_value NVARCHAR({}) NOT NULL,
		updated_at_utc DATETIME2(3) NOT NULL
			CONSTRAINT DF_server_configs_updated_at_utc
			DEFAULT SYSUTCDATETIME(),

		CONSTRAINT PK_server_configs
			PRIMARY KEY (config_key)
	);
END
)sql",
config::maxServerConfigKeyUtf16CodeUnitCount,
config::maxServerConfigValueUtf16CodeUnitCount
);

		odbc::OdbcStatement serverConfigStatement;
		const odbc::OdbcStatement::ExecuteResult createServerConfigsResult = serverConfigStatement.ExecuteDirect(connection, createServerConfigsTableQuery);
		if (!createServerConfigsResult.has_value())
		{
			return std::unexpected(createServerConfigsResult.error());
		}

		return {};
	}
}