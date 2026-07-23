#include "AccountServiceIntegrationTests.h"

#include <Windows.h>

#include <expected>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Core/PersistenceRuntime.h>
#include <Persistence/Odbc/OdbcConnection.h>
#include <Persistence/Odbc/OdbcEnvironment.h>
#include <Persistence/Odbc/OdbcStatement.h>

#include <Server/Account/AccountService.h>

#include <Tests/DebugTestResult.h>

namespace
{
	inline constexpr std::string_view databaseConnectionStringEnvironmentName
		= "WINDOWS_IOCP_UDP_TEST_DB_CONNECTION_STRING";

	using DeleteAccountResult
		= std::expected<void, persistence::core::DatabaseError>;

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

		const persistence::odbc::OdbcStatement::ExecuteResult prepareResult = statement.Prepare(
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

		const persistence::odbc::OdbcStatement::BindResult bindResult
			= statement.BindInputString(1, loginName);
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
}

namespace tests::server
{
	DebugTestResult RunAccountServiceIntegrationTests()
	{
		DebugTestResult result{};

		const std::optional<std::string> connectionString = ReadDatabaseConnectionString();
		if (!connectionString.has_value())
		{
			std::cout
				<< "[AccountServiceIntegration] Skipped: "
				<< databaseConnectionStringEnvironmentName
				<< " is not set.\n";

			return result;
		}

		persistence::PersistenceRuntime persistenceRuntime;

		const persistence::PersistenceRuntime::StartResult startResult = persistenceRuntime.Start(
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
				"AccountServiceIntegration: runtime start failed",
				startResult.error()
			);

			return result;
		}

		persistence::odbc::OdbcEnvironment cleanupEnvironment;

		const persistence::odbc::OdbcEnvironment::InitializeResult initializeResult
			= cleanupEnvironment.Initialize();
		if (!initializeResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountServiceIntegration: cleanup environment initialization failed",
				initializeResult.error()
			);

			return result;
		}

		persistence::odbc::OdbcConnection cleanupConnection;

		const persistence::odbc::OdbcConnection::OpenResult openResult = cleanupConnection.Open(
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
				"AccountServiceIntegration: cleanup connection failed",
				openResult.error()
			);

			return result;
		}

		constexpr std::string_view loginName
			= "account_service_duplicate_integration_test";

		const DeleteAccountResult initialDeleteResult
			= DeleteAccountByLoginName(cleanupConnection, loginName);
		if (!initialDeleteResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountServiceIntegration: initial cleanup failed",
				initialDeleteResult.error()
			);

			return result;
		}

		::server::account::AccountService accountService(persistenceRuntime);

		const ::server::account::CreateAccountResult firstCreateResult = accountService.CreateAccount(
			persistence::account::AccountCreateRequest{
				.loginName = loginName,
				.passwordHash = "integration_password_hash",
				.nickname = "IntegrationAccount",
			}
			);

		std::string firstCreateMessage
			= "AccountServiceIntegration: first account creation succeeds";

		if (!firstCreateResult.has_value())
		{
			const auto* databaseError
				= std::get_if<persistence::core::DatabaseError>(
					&firstCreateResult.error()
				);

			if (databaseError != nullptr)
			{
				firstCreateMessage += ": ";
				firstCreateMessage
					+= persistence::core::ToString(*databaseError);
			}
		}

		tests::Expect(
			result,
			firstCreateResult.has_value(),
			firstCreateMessage
		);

		if (firstCreateResult.has_value())
		{
			const ::server::account::CreateAccountResult duplicateCreateResult = accountService.CreateAccount(
				persistence::account::AccountCreateRequest{
					.loginName = loginName,
					.passwordHash = "another_password_hash",
					.nickname = "AnotherNickname",
				}
				);

			tests::Expect(
				result,
				!duplicateCreateResult.has_value(),
				"AccountServiceIntegration: duplicate account creation fails"
			);

			const auto* createAccountFailure
				= !duplicateCreateResult.has_value()
				? std::get_if<::server::account::CreateAccountFailure>(
					&duplicateCreateResult.error()
				)
				: nullptr;

			tests::Expect(
				result,
				createAccountFailure != nullptr
				&& *createAccountFailure
				== ::server::account::CreateAccountFailure::DuplicateLoginName,
				"AccountServiceIntegration: duplicate login name is classified"
			);

			const ::server::account::LoginAccountResult loginResult = accountService.LoginAccount(
				::server::account::AccountLoginRequest{
					.loginName = loginName,
					.passwordHash = "integration_password_hash",
				}
				);

			tests::Expect(
				result,
				loginResult.has_value(),
				"AccountServiceIntegration: account login succeeds"
			);

			tests::Expect(
				result,
				loginResult.has_value()
				&& loginResult->accountId == firstCreateResult->accountId,
				"AccountServiceIntegration: login account id matches"
			);

			tests::Expect(
				result,
				loginResult.has_value()
				&& loginResult->loginName == loginName,
				"AccountServiceIntegration: login name matches"
			);

			tests::Expect(
				result,
				loginResult.has_value()
				&& loginResult->nickname == "IntegrationAccount",
				"AccountServiceIntegration: login nickname matches"
			);

			const ::server::account::LoginAccountResult wrongPasswordResult = accountService.LoginAccount(
				::server::account::AccountLoginRequest{
					.loginName = loginName,
					.passwordHash = "wrong_password_hash",
				}
				);

			const auto* wrongPasswordFailure
				= !wrongPasswordResult.has_value()
				? std::get_if<::server::account::LoginAccountFailure>(
					&wrongPasswordResult.error()
				)
				: nullptr;

			tests::Expect(
				result,
				wrongPasswordFailure != nullptr
				&& *wrongPasswordFailure
				== ::server::account::LoginAccountFailure::InvalidCredentials,
				"AccountServiceIntegration: reject incorrect password hash"
			);

			const ::server::account::LoginAccountResult missingAccountResult = accountService.LoginAccount(
				::server::account::AccountLoginRequest{
					.loginName = "missing_account_login_test",
					.passwordHash = "integration_password_hash",
				}
				);

			const auto* missingAccountFailure
				= !missingAccountResult.has_value()
				? std::get_if<::server::account::LoginAccountFailure>(
					&missingAccountResult.error()
				)
				: nullptr;

			tests::Expect(
				result,
				missingAccountFailure != nullptr
				&& *missingAccountFailure
				== ::server::account::LoginAccountFailure::InvalidCredentials,
				"AccountServiceIntegration: hide missing account as invalid credentials"
			);
		}

		const DeleteAccountResult finalDeleteResult
			= DeleteAccountByLoginName(cleanupConnection, loginName);
		if (!finalDeleteResult.has_value())
		{
			AddDatabaseFailure(
				result,
				"AccountServiceIntegration: final cleanup failed",
				finalDeleteResult.error()
			);
		}

		persistenceRuntime.Stop();

		return result;
	}
}