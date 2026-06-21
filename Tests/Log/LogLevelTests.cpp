#include "LogLevelTests.h"

#include <Common/Log/LogLevel.h>

#include <Tests/TestHelpers.h>

namespace tests::log
{
	tests::DebugTestResult RunLogLevelTests()
	{
		tests::DebugTestResult result{};

		tests::Expect(result, common::log::ToString(common::log::LogLevel::Trace) == "Trace", "LogLevel: trace string");
		tests::Expect(result, common::log::ToString(common::log::LogLevel::Debug) == "Debug", "LogLevel: debug string");
		tests::Expect(result, common::log::ToString(common::log::LogLevel::Info) == "Info", "LogLevel: info string");
		tests::Expect(result, common::log::ToString(common::log::LogLevel::Warning) == "Warning", "LogLevel: warning string");
		tests::Expect(result, common::log::ToString(common::log::LogLevel::Error) == "Error", "LogLevel: error string");
		tests::Expect(
			result,
			common::log::TryParseLogLevel("Trace").value_or(common::log::LogLevel::Info) == common::log::LogLevel::Trace,
			"LogLevel: parse trace"
		);

		tests::Expect(
			result,
			common::log::TryParseLogLevel("debug").value_or(common::log::LogLevel::Info) == common::log::LogLevel::Debug,
			"LogLevel: parse debug"
		);

		tests::Expect(
			result,
			common::log::TryParseLogLevel(" INFO ").value_or(common::log::LogLevel::Trace) == common::log::LogLevel::Info,
			"LogLevel: parse trimmed info"
		);

		tests::Expect(
			result,
			common::log::TryParseLogLevel("warning").value_or(common::log::LogLevel::Info) == common::log::LogLevel::Warning,
			"LogLevel: parse warning"
		);

		tests::Expect(
			result,
			common::log::TryParseLogLevel("warn").value_or(common::log::LogLevel::Info) == common::log::LogLevel::Warning,
			"LogLevel: parse warn alias"
		);

		tests::Expect(
			result,
			common::log::TryParseLogLevel("error").value_or(common::log::LogLevel::Info) == common::log::LogLevel::Error,
			"LogLevel: parse error"
		);

		tests::Expect(
			result,
			!common::log::TryParseLogLevel("verbose").has_value(),
			"LogLevel: reject unknown value"
		);

		return result;
	}
}