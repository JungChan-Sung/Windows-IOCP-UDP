#include "DatabaseSchema.h"

#include <format>
#include <string>

#include <Persistence/Account/AccountConstraints.h>
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

		return {};
	}
}