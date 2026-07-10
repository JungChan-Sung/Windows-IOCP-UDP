#include "ConfigTextTests.h"

#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include <Common/Config/ConfigText.h>

namespace
{
	void RunTrimTest(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			common::config::Trim("  abc \t\n") == "abc",
			"ConfigText: trim whitespace"
		);
		tests::Expect(
			result,
			common::config::Trim("   ").empty(),
			"ConfigText: trim all whitespace"
		);
		tests::Expect(
			result,
			common::config::Trim("abc") == "abc",
			"ConfigText: trim unchanged"
		);
	}

	void RunRemoveCommentTest(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			common::config::RemoveComment("Value=1 # comment") == "Value=1 ",
			"ConfigText: remove inline hash comment"
		);

		tests::Expect(
			result,
			common::config::RemoveComment("; comment").empty(),
			"ConfigText: remove semicolon line comment"
		);

		tests::Expect(
			result,
			common::config::RemoveComment("   ; comment").empty(),
			"ConfigText: remove indented semicolon line comment"
		);

		tests::Expect(
			result,
			common::config::RemoveComment("Value=1 ; comment") == "Value=1 ; comment",
			"ConfigText: preserve inline semicolon"
		);

		tests::Expect(
			result,
			common::config::RemoveComment(
				"ConnectionString=Driver={ODBC Driver 18 for SQL Server};Server=localhost;Database=WindowsIocpUdp;"
			)
			== "ConnectionString=Driver={ODBC Driver 18 for SQL Server};Server=localhost;Database=WindowsIocpUdp;",
			"ConfigText: preserve ODBC connection string semicolons"
		);

		tests::Expect(
			result,
			common::config::RemoveComment("Value=1") == "Value=1",
			"ConfigText: no comment unchanged"
		);
	}

	void RunToLowerCopyTest(tests::DebugTestResult& result)
	{
		const std::string lowerText = common::config::ToLowerCopy("AbC123");

		tests::Expect(
			result,
			lowerText == "abc123",
			"ConfigText: lower copy"
		);
	}

	void RunTryParseUnsignedTest(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			common::config::TryParseUnsigned("123").value_or(0ULL) == 123ULL,
			"ConfigText: parse unsigned"
		);
		tests::Expect(
			result,
			!common::config::TryParseUnsigned("-1").has_value(),
			"ConfigText: reject negative unsigned"
		);
		tests::Expect(
			result,
			!common::config::TryParseUnsigned("12abc").has_value(),
			"ConfigText: reject partial unsigned"
		);
	}

	void RunTryParseSignedTest(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			common::config::TryParseSigned("-123").value_or(0LL) == -123LL,
			"ConfigText: parse signed"
		);
		tests::Expect(
			result,
			common::config::TryParseSigned("123").value_or(0LL) == 123LL,
			"ConfigText: parse positive signed"
		);
		tests::Expect(
			result,
			!common::config::TryParseSigned("12abc").has_value(),
			"ConfigText: reject partial signed"
		);
	}

	void RunTryParseMillisecondsTest(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			common::config::TryParseMilliseconds("250").value_or(std::chrono::milliseconds(0)) == std::chrono::milliseconds(250),
			"ConfigText: parse milliseconds"
		);
		tests::Expect(
			result,
			!common::config::TryParseMilliseconds("-1").has_value(),
			"ConfigText: reject negative milliseconds"
		);
	}

	void RunTryParseFloatTest(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			common::config::TryParseFloat("1.5").value_or(0.0F) == 1.5F,
			"ConfigText: parse float"
		);
		tests::Expect(
			result,
			!common::config::TryParseFloat("1.5abc").has_value(),
			"ConfigText: reject partial float"
		);
	}

	void RunTryParseBoolTest(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			common::config::TryParseBool("true").value_or(false),
			"ConfigText: parse bool true"
		);
		tests::Expect(
			result,
			common::config::TryParseBool("on").value_or(false),
			"ConfigText: parse bool on"
		);
		tests::Expect(
			result,
			!common::config::TryParseBool("false").value_or(true),
			"ConfigText: parse bool false"
		);
		tests::Expect(
			result,
			!common::config::TryParseBool("invalid").has_value(),
			"ConfigText: reject invalid bool"
		);
	}
}

namespace tests::config
{
	tests::DebugTestResult RunConfigTextTests()
	{
		tests::DebugTestResult result{};

		RunTrimTest(result);
		RunRemoveCommentTest(result);
		RunToLowerCopyTest(result);
		RunTryParseUnsignedTest(result);
		RunTryParseSignedTest(result);
		RunTryParseMillisecondsTest(result);
		RunTryParseFloatTest(result);
		RunTryParseBoolTest(result);

		return result;
	}
}