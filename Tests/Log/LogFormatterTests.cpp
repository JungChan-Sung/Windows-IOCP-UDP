#include "LogFormatterTests.h"

#include <chrono>
#include <string>

#include <Common/Log/LogFormatter.h>
#include <Common/Log/LogLevel.h>

#include <Tests/TestHelpers.h>

namespace tests::log
{
	void RunTimestampFormatTest(tests::DebugTestResult& result)
	{
		const std::chrono::system_clock::time_point timestamp =
			std::chrono::sys_days(std::chrono::year(2026) / std::chrono::June / 23)
			+ std::chrono::hours(12)
			+ std::chrono::minutes(34)
			+ std::chrono::seconds(56)
			+ std::chrono::milliseconds(123);

		const common::log::LogRecord logRecord =
			common::log::MakeLogRecord(timestamp, common::log::LogLevel::Info, "timestamp test");

		tests::Expect(
			result,
			common::log::FormatLogMessage(logRecord) == "[2026-06-23 12:34:56.123][Info] timestamp test",
			"LogFormatter: timestamp record message"
		);
	}

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

		RunTimestampFormatTest(result);

		return result;
	}
}