#pragma once

#include <expected>
#include <variant>

#include <Persistence/Account/AccountRepository.h>
#include <Persistence/Account/AccountValidation.h>
#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Core/PersistenceRuntime.h>

namespace server::account
{
	using CreateAccountError = std::variant<persistence::account::AccountValidationError, persistence::core::DatabaseError>;

	using CreateAccountResult = std::expected<persistence::account::AccountRecord, CreateAccountError>;

	class AccountService final
	{
	private:
		persistence::PersistenceRuntime& persistenceRuntime_;

	public:
		explicit AccountService(persistence::PersistenceRuntime& persistenceRuntime) noexcept;
		~AccountService() noexcept = default;

		AccountService(const AccountService&) = delete;
		AccountService& operator=(const AccountService&) = delete;

		AccountService(AccountService&&) = delete;
		AccountService& operator=(AccountService&&) = delete;

	public:
		[[nodiscard]] CreateAccountResult CreateAccount(const persistence::account::AccountCreateRequest& request);
	};
}