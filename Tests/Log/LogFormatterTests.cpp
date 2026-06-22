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

		tests::Expect(
			result,
			common::log::FormatLogMessage(
				common::log::LogRecord{
					.logLevel = common::log::LogLevel::Debug,
					.message = "record message",
				}
				) == "[Debug] record message",
			"LogFormatter: record message"
		);

		const common::log::LogRecord logRecord =
			common::log::MakeLogRecord(common::log::LogLevel::Warning, "copied message");

		tests::Expect(
			result,
			logRecord.logLevel == common::log::LogLevel::Warning,
			"LogRecord: level copied"
		);

		tests::Expect(
			result,
			logRecord.message == "copied message",
			"LogRecord: message copied"
		);

		return result;
	}
}