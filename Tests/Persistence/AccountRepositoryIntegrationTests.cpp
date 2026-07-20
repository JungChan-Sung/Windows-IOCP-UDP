#include "AccountRepositoryIntegrationTests.h"

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
#include <Persistence/Schema/DatabaseSchema.h>

#include <Tests/DebugTestResult.h>

namespace tests::persistence::detail
{
	inline constexpr std::string_view databaseConnectionStringEnvironmentName
		= "WINDOWS_IOCP_UDP_TEST_DB_CONNECTION_STRING";

	using DeleteAccountResult = std::expected<void, ::persistence::core::DatabaseError>;

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

	void AddDatabaseFailure(
		DebugTestResult& result,
		std::string_view operationName,
		const ::persistence::core::DatabaseError& error
	)
	{
		std::string message(operationName);
		message += ": ";
		message += ::persistence::core::ToString(error);

		result.AddFailed(message);
	}

	[[nodiscard]] DeleteAccountResult DeleteAccountByLoginName(
		::persistence::odbc::OdbcConnection& connection,
		std::string_view loginName
	)
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

		const ::persistence::odbc::OdbcStatement::BindResult bindResult = statement.BindInputString(
			static_cast<SQLUSMALLINT>(1),
			loginName
		);
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

	void RunAccountRoundTripTest(
		DebugTestResult& result,
		::persistence::account::AccountRepository& repository,
		::persistence::odbc::OdbcConnection& connection,
		std::string_view loginName,
		std::string_view passwordHash,
		std::string_view nickname,
		std::string_view testName
	)
	{
		const DeleteAccountResult initialDeleteResult
			= DeleteAccountByLoginName(
				connection,
				loginName
			);

		if (!initialDeleteResult.has_value())
		{
			AddDatabaseFailure(
				result,
				std::string{ testName }
				+ ": initial account cleanup failed",
				initialDeleteResult.error()
			);

			return;
		}

		const ::persistence::account::AccountRepository::ExistsResult
			existsBeforeCreateResult
			= repository.ExistsByLoginName(loginName);

		if (!existsBeforeCreateResult.has_value())
		{
			AddDatabaseFailure(
				result,
				std::string{ testName }
				+ ": initial account existence check failed",
				existsBeforeCreateResult.error()
			);

			return;
		}

		tests::Expect(
			result,
			!*existsBeforeCreateResult,
			std::string{ testName }
			+ ": account does not exist before creation"
		);

		const ::persistence::account::AccountRepository::CreateAccountResult
			createResult
			= repository.CreateAccount(
				::persistence::account::AccountCreateRequest{
					.loginName = loginName,
					.passwordHash = passwordHash,
					.nickname = nickname,
				}
				);

		if (!createResult.has_value())
		{
			AddDatabaseFailure(
				result,
				std::string{ testName }
				+ ": account creation failed",
				createResult.error()
			);

			return;
		}

		tests::Expect(
			result,
			createResult->accountId > 0,
			std::string{ testName }
			+ ": created account id is valid"
		);

		const ::persistence::account::AccountRepository::ExistsResult
			existsAfterCreateResult
			= repository.ExistsByLoginName(loginName);

		if (!existsAfterCreateResult.has_value())
		{
			AddDatabaseFailure(
				result,
				std::string{ testName }
				+ ": account existence check failed",
				existsAfterCreateResult.error()
			);

			return;
		}

		tests::Expect(
			result,
			*existsAfterCreateResult,
			std::string{ testName }
			+ ": account exists after creation"
		);

		const ::persistence::account::AccountRepository::FindAccountResult
			findResult
			= repository.FindAccountByLoginName(loginName);

		if (!findResult.has_value())
		{
			AddDatabaseFailure(
				result,
				std::string{ testName }
				+ ": account lookup failed",
				findResult.error()
			);

			return;
		}

		tests::Expect(
			result,
			findResult->has_value(),
			std::string{ testName }
			+ ": created account can be found"
		);

		if (findResult->has_value())
		{
			const ::persistence::account::AccountRecord& account
				= **findResult;

			tests::Expect(
				result,
				account.accountId == createResult->accountId,
				std::string{ testName }
				+ ": found account id matches"
			);

			tests::Expect(
				result,
				account.loginName == loginName,
				std::string{ testName }
				+ ": found login name matches"
			);

			tests::Expect(
				result,
				account.passwordHash == passwordHash,
				std::string{ testName }
				+ ": found password hash matches"
			);

			tests::Expect(
				result,
				account.nickname == nickname,
				std::string{ testName }
				+ ": found nickname matches"
			);
		}

		const DeleteAccountResult finalDeleteResult
			= DeleteAccountByLoginName(
				connection,
				loginName
			);

		if (!finalDeleteResult.has_value())
		{
			AddDatabaseFailure(
				result,
				std::string{ testName }
				+ ": final account cleanup failed",
				finalDeleteResult.error()
			);

			return;
		}

		const ::persistence::account::AccountRepository::ExistsResult
			existsAfterDeleteResult
			= repository.ExistsByLoginName(loginName);

		if (!existsAfterDeleteResult.has_value())
		{
			AddDatabaseFailure(
				result,
				std::string{ testName }
				+ ": existence check after cleanup failed",
				existsAfterDeleteResult.error()
			);

			return;
		}

		tests::Expect(
			result,
			!*existsAfterDeleteResult,
			std::string{ testName }
			+ ": account does not exist after cleanup"
		);
	}
}

namespace tests::persistence
{
	DebugTestResult RunAccountRepositoryIntegrationTests()
	{
		DebugTestResult result{};

		const std::optional<std::string> connectionString = detail::ReadDatabaseConnectionString();
		if (!connectionString.has_value())
		{
			std::cout
				<< "[AccountRepositoryIntegration] Skipped: "
				<< detail::databaseConnectionStringEnvironmentName
				<< " is not set.\n";

			return result;
		}

		::persistence::odbc::OdbcEnvironment environment;

		const ::persistence::odbc::OdbcEnvironment::InitializeResult initializeResult = environment.Initialize();
		if (!initializeResult.has_value())
		{
			detail::AddDatabaseFailure(result, "Environment initialization failed", initializeResult.error());
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
			detail::AddDatabaseFailure(result, "Database connection failed", openResult.error());
			return result;
		}

		const ::persistence::schema::DatabaseSchema::InitializeResult schemaResult
			= ::persistence::schema::DatabaseSchema::Initialize(connection);
		if (!schemaResult.has_value())
		{
			detail::AddDatabaseFailure(result, "Database schema initialization failed", schemaResult.error());
			return result;
		}

		constexpr std::string_view loginName = "account_repository_integration_test";
		constexpr std::string_view passwordHash = "integration_test_hash";
		constexpr std::string_view nickname = "IntegrationTester";

		const detail::DeleteAccountResult initialDeleteResult = detail::DeleteAccountByLoginName(connection, loginName);
		if (!initialDeleteResult.has_value())
		{
			detail::AddDatabaseFailure(result, "Initial test account cleanup failed", initialDeleteResult.error());
			return result;
		}

		::persistence::account::AccountRepository repository(connection);

		detail::RunAccountRoundTripTest(
			result,
			repository,
			connection,
			"account_repository_integration_test",
			"integration_test_hash",
			"IntegrationTester",
			"AccountRepositoryIntegration ASCII"
		);

		detail::RunAccountRoundTripTest(
			result,
			repository,
			connection,
			"한글_계정_통합테스트",
			"한글_비밀번호_해시_123",
			"정찬_테스터",
			"AccountRepositoryIntegration Unicode"
		);

		return result;
	}
}