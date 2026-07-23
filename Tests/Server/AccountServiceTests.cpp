#include "AccountServiceTests.h"

#include <variant>

#include <Persistence/Account/AccountValidation.h>
#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Core/PersistenceRuntime.h>

#include <Server/Account/AccountService.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void RunCreateValidationFailureTest(tests::DebugTestResult& result)
	{
		persistence::PersistenceRuntime persistenceRuntime;
		server::account::AccountService accountService(persistenceRuntime);

		const server::account::CreateAccountResult createResult = accountService.CreateAccount(
			persistence::account::AccountCreateRequest{
				.loginName = "",
				.passwordHash = "password_hash",
				.nickname = "nickname",
			}
			);

		const auto* validationError
			= !createResult.has_value()
			? std::get_if<persistence::account::AccountValidationError>(
				&createResult.error()
			)
			: nullptr;

		tests::Expect(
			result,
			validationError != nullptr,
			"AccountService: create reports validation error"
		);

		tests::Expect(
			result,
			validationError != nullptr
			&& validationError->field
			== persistence::account::AccountField::LoginName
			&& validationError->failure
			== persistence::account::AccountValidationFailure::Empty,
			"AccountService: create preserves validation details"
		);
	}

	void RunLoginValidationFailureTest(tests::DebugTestResult& result)
	{
		persistence::PersistenceRuntime persistenceRuntime;
		server::account::AccountService accountService(persistenceRuntime);

		const server::account::LoginAccountResult loginResult = accountService.LoginAccount(
			server::account::AccountLoginRequest{
				.loginName = "account",
				.passwordHash = "",
			}
			);

		const auto* validationError
			= !loginResult.has_value()
			? std::get_if<persistence::account::AccountValidationError>(
				&loginResult.error()
			)
			: nullptr;

		tests::Expect(
			result,
			validationError != nullptr,
			"AccountService: login reports validation error"
		);

		tests::Expect(
			result,
			validationError != nullptr
			&& validationError->field
			== persistence::account::AccountField::PasswordHash
			&& validationError->failure
			== persistence::account::AccountValidationFailure::Empty,
			"AccountService: login preserves validation details"
		);
	}

	void RunCreateDatabaseFailureTest(tests::DebugTestResult& result)
	{
		persistence::PersistenceRuntime persistenceRuntime;
		server::account::AccountService accountService(persistenceRuntime);

		const server::account::CreateAccountResult createResult = accountService.CreateAccount(
			persistence::account::AccountCreateRequest{
				.loginName = "account",
				.passwordHash = "password_hash",
				.nickname = "nickname",
			}
			);

		const auto* databaseError
			= !createResult.has_value()
			? std::get_if<persistence::core::DatabaseError>(
				&createResult.error()
			)
			: nullptr;

		tests::Expect(
			result,
			databaseError != nullptr,
			"AccountService: create reports database error"
		);

		tests::Expect(
			result,
			databaseError != nullptr
			&& databaseError->failure
			== persistence::core::DatabaseFailure::ConnectionOpenFailed,
			"AccountService: create preserves database failure"
		);
	}

	void RunLoginDatabaseFailureTest(tests::DebugTestResult& result)
	{
		persistence::PersistenceRuntime persistenceRuntime;
		server::account::AccountService accountService(persistenceRuntime);

		const server::account::LoginAccountResult loginResult = accountService.LoginAccount(
			server::account::AccountLoginRequest{
				.loginName = "account",
				.passwordHash = "password_hash",
			}
			);

		const auto* databaseError
			= !loginResult.has_value()
			? std::get_if<persistence::core::DatabaseError>(
				&loginResult.error()
			)
			: nullptr;

		tests::Expect(
			result,
			databaseError != nullptr,
			"AccountService: login reports database error"
		);

		tests::Expect(
			result,
			databaseError != nullptr
			&& databaseError->failure
			== persistence::core::DatabaseFailure::ConnectionOpenFailed,
			"AccountService: login preserves database failure"
		);
	}
}

namespace tests::server
{
	DebugTestResult RunAccountServiceTests()
	{
		DebugTestResult result{};

		RunCreateValidationFailureTest(result);
		RunLoginValidationFailureTest(result);
		RunCreateDatabaseFailureTest(result);
		RunLoginDatabaseFailureTest(result);

		return result;
	}
}