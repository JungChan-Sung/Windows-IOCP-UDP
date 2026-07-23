#pragma once

#include <cstdint>
#include <expected>
#include <variant>

#include <Persistence/Account/AccountRepository.h>
#include <Persistence/Account/AccountValidation.h>
#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Core/PersistenceRuntime.h>

namespace server::account
{
	enum class CreateAccountFailure
	{
		DuplicateLoginName,
	};

	using CreateAccountError = std::variant<persistence::account::AccountValidationError, CreateAccountFailure, persistence::core::DatabaseError>;

	using CreateAccountResult = std::expected<persistence::account::AccountRecord, CreateAccountError>;

	class AccountService final
	{
	private:
		inline static constexpr std::int32_t duplicateIndexNativeError = 2601;
		inline static constexpr std::int32_t uniqueConstraintNativeError = 2627;

	private:
		persistence::PersistenceRuntime& persistenceRuntime_;

	public:
		explicit AccountService(persistence::PersistenceRuntime& persistenceRuntime) noexcept;
		~AccountService() noexcept = default;

		AccountService(const AccountService&) = delete;
		AccountService& operator=(const AccountService&) = delete;

		AccountService(AccountService&&) = delete;
		AccountService& operator=(AccountService&&) = delete;

	private:
		[[nodiscard]] static bool IsDuplicateLoginNameError(const persistence::core::DatabaseError& databaseError) noexcept;

	public:
		[[nodiscard]] CreateAccountResult CreateAccount(const persistence::account::AccountCreateRequest& request);
	};
}