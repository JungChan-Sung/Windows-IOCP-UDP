#include "UtfConversionTests.h"

#include <string>
#include <string_view>

#include <Common/String/UtfConversion.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void RunRoundTripTest(
		tests::DebugTestResult& result,
		std::string_view sourceText,
		std::string_view testName
	)
	{
		const common::string::Utf16ConversionResult utf16Result
			= common::string::ConvertUtf8ToUtf16(sourceText);

		tests::Expect(
			result,
			utf16Result.has_value(),
			std::string{ testName } + ": UTF-8 to UTF-16"
		);

		if (!utf16Result.has_value())
		{
			return;
		}

		const common::string::Utf8ConversionResult utf8Result
			= common::string::ConvertUtf16ToUtf8(*utf16Result);

		tests::Expect(
			result,
			utf8Result.has_value(),
			std::string{ testName } + ": UTF-16 to UTF-8"
		);

		if (!utf8Result.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			*utf8Result == sourceText,
			std::string{ testName } + ": round trip"
		);
	}

	void RunEmptyTextTest(
		tests::DebugTestResult& result
	)
	{
		const common::string::Utf16ConversionResult utf16Result
			= common::string::ConvertUtf8ToUtf16("");

		const common::string::Utf8ConversionResult utf8Result
			= common::string::ConvertUtf16ToUtf8(L"");

		tests::Expect(
			result,
			utf16Result.has_value() && utf16Result->empty(),
			"UtfConversion: empty UTF-8"
		);

		tests::Expect(
			result,
			utf8Result.has_value() && utf8Result->empty(),
			"UtfConversion: empty UTF-16"
		);
	}

	void RunInvalidUtf8Test(
		tests::DebugTestResult& result
	)
	{
		const std::string invalidUtf8{
			static_cast<char>(0xC3),
			static_cast<char>(0x28),
		};

		const common::string::Utf16ConversionResult conversionResult
			= common::string::ConvertUtf8ToUtf16(invalidUtf8);

		tests::Expect(
			result,
			!conversionResult.has_value(),
			"UtfConversion: reject invalid UTF-8"
		);

		if (!conversionResult.has_value())
		{
			tests::Expect(
				result,
				conversionResult.error().failure
				== common::string::UtfConversionFailure::InvalidUtf8,
				"UtfConversion: report invalid UTF-8"
			);
		}
	}

	void RunInvalidUtf16Test(
		tests::DebugTestResult& result
	)
	{
		const std::wstring invalidUtf16(
			1,
			static_cast<wchar_t>(0xD800)
		);

		const common::string::Utf8ConversionResult conversionResult
			= common::string::ConvertUtf16ToUtf8(invalidUtf16);

		tests::Expect(
			result,
			!conversionResult.has_value(),
			"UtfConversion: reject invalid UTF-16"
		);

		if (!conversionResult.has_value())
		{
			tests::Expect(
				result,
				conversionResult.error().failure
				== common::string::UtfConversionFailure::InvalidUtf16,
				"UtfConversion: report invalid UTF-16"
			);
		}
	}
}

namespace tests::string
{
	DebugTestResult RunUtfConversionTests()
	{
		DebugTestResult result{};

		RunRoundTripTest(
			result,
			"account_test_123",
			"UtfConversion ASCII"
		);

		RunRoundTripTest(
			result,
			"정찬_테스트계정",
			"UtfConversion Korean"
		);

		RunEmptyTextTest(result);
		RunInvalidUtf8Test(result);
		RunInvalidUtf16Test(result);

		return result;
	}
}