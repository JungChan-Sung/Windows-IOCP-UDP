#include "ServerConfigRepository.h"

#include <Persistence/Odbc/OdbcStatement.h>

namespace persistence::config
{
	ServerConfigRepository::ServerConfigRepository(odbc::OdbcConnection& connection) noexcept
		: connection_(connection)
	{}

	ServerConfigRepository::LoadAllResult ServerConfigRepository::LoadAll()
	{
		if (!connection_.IsOpen())
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::ConnectionOpenFailed,
				.message = "Database connection is not open.",
				});
		}

		odbc::OdbcStatement statement;

		const odbc::OdbcStatement::ExecuteResult executeResult = statement.ExecuteDirect(
			connection_,
			R"sql(
SELECT
	config_key,
	config_value
FROM dbo.server_configs
ORDER BY config_key;
)sql"
);

		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		EntryList entryList;

		while (true)
		{
			const odbc::OdbcStatement::FetchResult fetchResult = statement.Fetch();
			if (!fetchResult.has_value())
			{
				return std::unexpected(fetchResult.error());
			}

			if (!*fetchResult)
			{
				break;
			}

			const odbc::OdbcStatement::ReadStringResult keyResult = statement.ReadString(static_cast<SQLUSMALLINT>(1));
			if (!keyResult.has_value())
			{
				return std::unexpected(keyResult.error());
			}

			const odbc::OdbcStatement::ReadStringResult valueResult = statement.ReadString(static_cast<SQLUSMALLINT>(2));
			if (!valueResult.has_value())
			{
				return std::unexpected(valueResult.error());
			}

			entryList.push_back(ServerConfigEntry{
				.key = *keyResult,
				.value = *valueResult,
				});
		}

		return entryList;
	}
}