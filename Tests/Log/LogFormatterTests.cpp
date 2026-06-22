#include "LogFormatterTests.h"

#include <chrono>
#include <ctime>
#include <string>

#include <Common/Log/LogFormatter.h>
#include <Common/Log/LogLevel.h>
#include <Common/Log/LogRecord.h>
#include <Common/Time/TimeTypes.h>

#include <Tests/TestHelpers.h>

namespace
{
	[[nodiscard]] common::time::SystemTimePoint MakeLocalTimestamp()
	{
		std::tm localTime{};
		localTime.tm_year = 2026 - 1900;
		localTime.tm_mon = 6 - 1;
		localTime.tm_mday = 23;
		localTime.tm_hour = 12;
		localTime.tm_min = 34;
		localTime.tm_sec = 56;
		localTime.tm_isdst = -1;

		const std::time_t timeValue = std::mktime(&localTime);

		return common::time::SystemClock::from_time_t(timeValue)
			+ common::time::Milliseconds(123);
	}

	void RunDynamicFormatTest(tests::DebugTestResult& result)
	{
		const std::string formattedMessage =
			common::log::FormatLogMessage(common::log::LogLevel::Info, "hello");

		tests::Expect(
			result,
			formattedMessage.starts_with("["),
			"LogFormatter: dynamic message starts with timestamp"
		);

		tests::Expect(
			result,
			formattedMessage.find("][Info] hello") != std::string::npos,
			"LogFormatter: dynamic message contains level and message"
		);
	}

	void RunWarningFormatTest(tests::DebugTestResult& result)
	{
		const std::string formattedMessage =
			common::log::FormatLogMessage(common::log::LogLevel::Warning, "visible warning log");

		tests::Expect(
			result,
			formattedMessage.find("][Warning] visible warning log") != std::string::npos,
			"LogFormatter: warning message contains level and message"
		);
	}

	void RunEmptyMessageFormatTest(tests::DebugTestResult& result)
	{
		const std::string formattedMessage =
			common::log::FormatLogMessage(common::log::LogLevel::Error, "");

		tests::Expect(
			result,
			formattedMessage.find("][Error] ") != std::string::npos,
			"LogFormatter: empty message contains level"
		);
	}

	void RunFixedTimestampFormatTest(tests::DebugTestResult& result)
	{
		const common::log::LogRecord logRecord =
			common::log::MakeLogRecord(
				MakeLocalTimestamp(),
				common::log::LogLevel::Debug,
				"fixed timestamp"
			);

		tests::Expect(
			result,
			common::log::FormatLogMessage(logRecord) == "[2026-06-23 12:34:56.123][Debug] fixed timestamp",
			"LogFormatter: fixed timestamp record message"
		);
	}

	void RunMakeLogRecordTest(tests::DebugTestResult& result)
	{
		const common::time::SystemTimePoint before = common::time::SystemClock::now();

		const common::log::LogRecord logRecord =
			common::log::MakeLogRecord(common::log::LogLevel::Warning, "copied message");

		const common::time::SystemTimePoint after = common::time::SystemClock::now();

		tests::Expect(
			result,
			logRecord.timestamp >= before && logRecord.timestamp <= after,
			"LogRecord: timestamp captured"
		);

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
	}
}

namespace tests::log
{
	tests::DebugTestResult RunLogFormatterTests()
	{
		tests::DebugTestResult result{};

		RunDynamicFormatTest(result);
		RunWarningFormatTest(result);
		RunEmptyMessageFormatTest(result);
		RunFixedTimestampFormatTest(result);
		RunMakeLogRecordTest(result);

		return result;
	}
}