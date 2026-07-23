#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

namespace persistence::account
{
	enum class AccountField
	{
		LoginName,
		PasswordHash,
		Nickname,
	};

	enum class AccountValidationFailure
	{
		Empty,
		InvalidUtf8,
		ContainsNullCharacter,
		TooLong,
	};

	struct AccountValidationError
	{
	public:
		AccountField field = AccountField::LoginName;

		AccountValidationFailure failure = AccountValidationFailure::Empty;

		std::size_t actualUtf16CodeUnitCount = 0;
		std::size_t maximumUtf16CodeUnitCount = 0;

		std::uint32_t nativeError = 0;
	};

	using AccountValidationResult = std::expected<void, AccountValidationError>;

	[[nodiscard]] AccountValidationResult ValidateAccountCreateFields(
		std::string_view loginName,
		std::string_view passwordHash,
		std::string_view nickname
	);
	[[nodiscard]] AccountValidationResult ValidateAccountLoginFields(std::string_view loginName, std::string_view passwordHash);
}