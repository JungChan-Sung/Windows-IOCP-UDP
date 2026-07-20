#include "AccountValidationTests.h"

#include <string>

#include <Persistence/Account/AccountValidation.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using ValidationResult
		= ::persistence::account::AccountValidationResult;

	void ExpectFailure(
		tests::DebugTestResult& result,
		const ValidationResult& validationResult,
		::persistence::account::AccountField expectedField,
		::persistence::account::AccountValidationFailure
		expectedFailure,
		std::string_view testName
	)
	{
		const bool matches
			= !validationResult.has_value()
			&& validationResult.error().field
			== expectedField
			&& validationResult.error().failure
			== expectedFailure;

		tests::Expect(result, matches, testName);
	}
}

namespace tests::persistence
{
	DebugTestResult RunAccountValidationTests()
	{
		using namespace ::persistence::account;

		DebugTestResult result{};

		tests::Expect(
			result,
			ValidateAccountCreateFields(
				"account",
				"password_hash",
				"nickname"
			).has_value(),
			"AccountValidation: accept ASCII fields"
		);

		tests::Expect(
			result,
			ValidateAccountCreateFields(
				"한글계정",
				"한글_해시",
				"한글별명"
			).has_value(),
			"AccountValidation: accept Unicode fields"
		);

		ExpectFailure(
			result,
			ValidateAccountCreateFields(
				"",
				"hash",
				"nickname"
			),
			AccountField::LoginName,
			AccountValidationFailure::Empty,
			"AccountValidation: reject empty login name"
		);

		ExpectFailure(
			result,
			ValidateAccountCreateFields(
				std::string(51, 'a'),
				"hash",
				"nickname"
			),
			AccountField::LoginName,
			AccountValidationFailure::TooLong,
			"AccountValidation: reject long login name"
		);

		std::string maximumEmojiLoginName;
		for (int index = 0; index < 25; ++index)
		{
			maximumEmojiLoginName += "😀";
		}

		tests::Expect(
			result,
			ValidateAccountCreateFields(
				maximumEmojiLoginName,
				"hash",
				"nickname"
			).has_value(),
			"AccountValidation: accept 50 UTF-16 code units"
		);

		maximumEmojiLoginName += "😀";

		ExpectFailure(
			result,
			ValidateAccountCreateFields(
				maximumEmojiLoginName,
				"hash",
				"nickname"
			),
			AccountField::LoginName,
			AccountValidationFailure::TooLong,
			"AccountValidation: reject more than "
			"50 UTF-16 code units"
		);

		const std::string invalidUtf8{
			static_cast<char>(0xC3),
			static_cast<char>(0x28),
		};

		ExpectFailure(
			result,
			ValidateAccountCreateFields(
				invalidUtf8,
				"hash",
				"nickname"
			),
			AccountField::LoginName,
			AccountValidationFailure::InvalidUtf8,
			"AccountValidation: reject invalid UTF-8"
		);

		const std::string embeddedNullLoginName(
			"abc\0def",
			7
		);

		ExpectFailure(
			result,
			ValidateAccountCreateFields(
				embeddedNullLoginName,
				"hash",
				"nickname"
			),
			AccountField::LoginName,
			AccountValidationFailure
			::ContainsNullCharacter,
			"AccountValidation: reject embedded null"
		);

		return result;
	}
}