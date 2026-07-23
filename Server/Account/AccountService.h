#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
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

	enum class LoginAccountFailure
	{
		InvalidCredentials,
	};

	struct AccountLoginRequest
	{
	public:
		std::string_view loginName;
		std::string_view passwordHash;
	};

	struct AccountLoginRecord
	{
	public:
		std::int64_t accountId = 0;
		std::string loginName;
		std::string nickname;
	};

	using CreateAccountError = std::variant<persistence::account::AccountValidationError, CreateAccountFailure, persistence::core::DatabaseError>;
	using CreateAccountResult = std::expected<persistence::account::AccountRecord, CreateAccountError>;

	using LoginAccountError = std::variant<persistence::account::AccountValidationError, LoginAccountFailure, persistence::core::DatabaseError>;
	using LoginAccountResult = std::expected<AccountLoginRecord, LoginAccountError>;

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
		[[nodiscard]] LoginAccountResult LoginAccount(const AccountLoginRequest& request);
	};
}