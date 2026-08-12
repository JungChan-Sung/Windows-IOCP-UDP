#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include <Common/Identity/IdentityTypes.h>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>

namespace persistence::account
{
	struct AccountCreateRequest
	{
	public:
		std::string_view loginName;
		std::string_view passwordHash;
		std::string_view nickname;
	};

	struct AccountRecord
	{
	public:
		common::identity::AccountId accountId = 0;
		std::string loginName;
		std::string passwordHash;
		std::string nickname;
	};

	class AccountRepository final
	{
	public:
		using CreateAccountResult = std::expected<AccountRecord, core::DatabaseError>;
		using FindAccountResult = std::expected<std::optional<AccountRecord>, core::DatabaseError>;
		using ExistsResult = std::expected<bool, core::DatabaseError>;

	private:
		odbc::OdbcConnection& connection_;

	public:
		explicit AccountRepository(odbc::OdbcConnection& connection) noexcept;
		~AccountRepository() noexcept = default;

		AccountRepository(const AccountRepository&) = delete;
		AccountRepository& operator=(const AccountRepository&) = delete;

		AccountRepository(AccountRepository&&) = delete;
		AccountRepository& operator=(AccountRepository&&) = delete;

	public:
		[[nodiscard]] CreateAccountResult CreateAccount(const AccountCreateRequest& request);
		[[nodiscard]] FindAccountResult FindAccountByLoginName(std::string_view loginName);
		[[nodiscard]] ExistsResult ExistsByLoginName(std::string_view loginName);
	};
}