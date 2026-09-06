#include "AccountRepository.h"

#include <Persistence/Odbc/OdbcStatement.h>

namespace persistence::account
{
	AccountRepository::AccountRepository(odbc::OdbcConnection& connection) noexcept
		: connection_(connection)
	{}

	AccountRepository::CreateAccountResult AccountRepository::CreateAccount(const AccountCreateRequest& request)
	{
		if (!connection_.IsOpen())
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::ConnectionOpenFailed,
				.message = "Database connection is not open.",
				});
		}

		odbc::OdbcStatement statement;

		// INSERT와 생성된 IDENTITY 조회를 한 Statement에서 처리해 새 AccountId를 반환
		const odbc::OdbcStatement::ExecuteResult prepareResult = statement.Prepare(
			connection_,
			R"sql(
INSERT INTO dbo.accounts
(
	login_name,
	password_hash,
	nickname
)
OUTPUT INSERTED.account_id
VALUES
(
	?,
	?,
	?
);
)sql"
);
		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const odbc::OdbcStatement::BindResult loginNameBindResult = statement.BindInputString(static_cast<SQLUSMALLINT>(1), request.loginName);
		if (!loginNameBindResult.has_value())
		{
			return std::unexpected(loginNameBindResult.error());
		}

		const odbc::OdbcStatement::BindResult passwordHashBindResult = statement.BindInputString(static_cast<SQLUSMALLINT>(2), request.passwordHash);
		if (!passwordHashBindResult.has_value())
		{
			return std::unexpected(passwordHashBindResult.error());
		}

		const odbc::OdbcStatement::BindResult nicknameBindResult = statement.BindInputString(static_cast<SQLUSMALLINT>(3), request.nickname);
		if (!nicknameBindResult.has_value())
		{
			return std::unexpected(nicknameBindResult.error());
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
				.message = "Created account id was not returned.",
				});
		}

		const odbc::OdbcStatement::ReadInt64Result accountIdResult = statement.ReadInt64(static_cast<SQLUSMALLINT>(1));
		if (!accountIdResult.has_value())
		{
			return std::unexpected(accountIdResult.error());
		}

		return AccountRecord{
			.accountId = *accountIdResult,
			.loginName = std::string(request.loginName),
			.passwordHash = std::string(request.passwordHash),
			.nickname = std::string(request.nickname),
		};
	}

	AccountRepository::FindAccountResult AccountRepository::FindAccountByLoginName(std::string_view loginName)
	{
		if (!connection_.IsOpen())
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::ConnectionOpenFailed,
				.message = "Database connection is not open.",
				});
		}

		odbc::OdbcStatement statement;

		const odbc::OdbcStatement::ExecuteResult prepareResult = statement.Prepare(
			connection_,
			R"sql(
SELECT
	account_id,
	login_name,
	password_hash,
	nickname
FROM dbo.accounts
WHERE login_name = ?;
)sql"
);
		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const odbc::OdbcStatement::BindResult bindResult = statement.BindInputString(static_cast<SQLUSMALLINT>(1), loginName);
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

		const odbc::OdbcStatement::ReadInt64Result accountIdResult = statement.ReadInt64(static_cast<SQLUSMALLINT>(1));
		if (!accountIdResult.has_value())
		{
			return std::unexpected(accountIdResult.error());
		}

		const odbc::OdbcStatement::ReadStringResult loginNameResult = statement.ReadString(static_cast<SQLUSMALLINT>(2));
		if (!loginNameResult.has_value())
		{
			return std::unexpected(loginNameResult.error());
		}

		const odbc::OdbcStatement::ReadStringResult passwordHashResult = statement.ReadString(static_cast<SQLUSMALLINT>(3));
		if (!passwordHashResult.has_value())
		{
			return std::unexpected(passwordHashResult.error());
		}

		const odbc::OdbcStatement::ReadStringResult nicknameResult = statement.ReadString(static_cast<SQLUSMALLINT>(4));
		if (!nicknameResult.has_value())
		{
			return std::unexpected(nicknameResult.error());
		}

		return AccountRecord{
			.accountId = *accountIdResult,
			.loginName = *loginNameResult,
			.passwordHash = *passwordHashResult,
			.nickname = *nicknameResult,
		};
	}

	AccountRepository::ExistsResult AccountRepository::ExistsByLoginName(std::string_view loginName)
	{
		if (!connection_.IsOpen())
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::ConnectionOpenFailed,
				.message = "Database connection is not open.",
				});
		}

		odbc::OdbcStatement statement;

		const odbc::OdbcStatement::ExecuteResult prepareResult = statement.Prepare(
			connection_,
			R"sql(
SELECT 1
FROM dbo.accounts
WHERE login_name = ?;
)sql"
);
		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const odbc::OdbcStatement::BindResult bindResult = statement.BindInputString(static_cast<SQLUSMALLINT>(1), loginName);
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

		return *fetchResult;
	}
}