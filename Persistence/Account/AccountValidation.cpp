#include "AccountValidation.h"

#include <optional>
#include <string_view>

#include <Common/String/UtfConversion.h>

#include "AccountConstraints.h"

namespace
{
	[[nodiscard]] std::optional<persistence::account::AccountValidationError> ValidateField(
		std::string_view text,
		persistence::account::AccountField field,
		std::size_t maximumUtf16CodeUnitCount
	)
	{
		using namespace persistence::account;

		if (text.empty())
		{
			return AccountValidationError{
				.field = field,
				.failure = AccountValidationFailure::Empty,
				.maximumUtf16CodeUnitCount
					= maximumUtf16CodeUnitCount,
			};
		}

		// ODBC 문자열 바인딩이 SQL_NTS를 사용하므로 embedded NUL에 의한 조기 문자열 종료를 거부
		if (text.find('\0') != std::string_view::npos)
		{
			return AccountValidationError{
				.field = field,
				.failure = AccountValidationFailure::ContainsNullCharacter,
				.maximumUtf16CodeUnitCount = maximumUtf16CodeUnitCount,
			};
		}

		const common::string::Utf16ConversionResult utf16Result = common::string::ConvertUtf8ToUtf16(text);
		if (!utf16Result.has_value())
		{
			return AccountValidationError{
				.field = field,
				.failure = AccountValidationFailure::InvalidUtf8,
				.maximumUtf16CodeUnitCount = maximumUtf16CodeUnitCount,
				.nativeError = utf16Result.error().nativeError,
			};
		}

		// NVARCHAR(N)과 동일한 기준으로 제한하기 위해 UTF-16 Code Unit 수를 검사
		const std::size_t actualCodeUnitCount = utf16Result->size();
		if (actualCodeUnitCount > maximumUtf16CodeUnitCount)
		{
			return AccountValidationError{
				.field = field,
				.failure = AccountValidationFailure::TooLong,
				.actualUtf16CodeUnitCount = actualCodeUnitCount,
				.maximumUtf16CodeUnitCount = maximumUtf16CodeUnitCount,
			};
		}

		return std::nullopt;
	}
}

namespace persistence::account
{
	AccountValidationResult ValidateAccountCreateFields(std::string_view loginName, std::string_view passwordHash, std::string_view nickname)
	{
		if (const auto error = ValidateField(loginName, AccountField::LoginName, maxLoginNameUtf16CodeUnitCount))
		{
			return std::unexpected(*error);
		}

		if (const auto error = ValidateField(passwordHash, AccountField::PasswordHash, maxPasswordHashUtf16CodeUnitCount))
		{
			return std::unexpected(*error);
		}

		if (const auto error = ValidateField(nickname, AccountField::Nickname, maxNicknameUtf16CodeUnitCount))
		{
			return std::unexpected(*error);
		}

		return {};
	}

	AccountValidationResult ValidateAccountLoginFields(std::string_view loginName, std::string_view passwordHash)
	{
		if (const auto error = ValidateField(loginName, AccountField::LoginName, maxLoginNameUtf16CodeUnitCount))
		{
			return std::unexpected(*error);
		}

		if (const auto error = ValidateField(passwordHash, AccountField::PasswordHash, maxPasswordHashUtf16CodeUnitCount))
		{
			return std::unexpected(*error);
		}

		return {};
	}
}