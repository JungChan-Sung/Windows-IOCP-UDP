#include "LogFormatterTests.h"
#include "LogFormatterTests.h"

#include <Common/Log/LogFormatter.h>
#include <Common/Log/LogLevel.h>

#include <Tests/TestHelpers.h>

namespace tests::log
{
	tests::DebugTestResult RunLogFormatterTests()
	{
		tests::DebugTestResult result{};

		tests::Expect(
			result,
			common::log::FormatLogMessage(common::log::LogLevel::Info, "hello") == "[Info] hello",
			"LogFormatter: info message"
		);

		tests::Expect(
			result,
			common::log::FormatLogMessage(common::log::LogLevel::Warning, "visible warning log") == "[Warning] visible warning log",
			"LogFormatter: warning message"
		);

		tests::Expect(
			result,
			common::log::FormatLogMessage(common::log::LogLevel::Error, "") == "[Error] ",
			"LogFormatter: empty message"
		);

		return result;
	}
}