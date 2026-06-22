#pragma once

#include <string>
#include <string_view>

#include <Common/Log/LogLevel.h>
#include <Common/Time/TimeTypes.h>

namespace common::log
{
	struct LogRecord
	{
	public:
		time::SystemTimePoint timestamp = time::SystemClock::now();
		LogLevel logLevel = LogLevel::Info;
		std::string message;
	};

	[[nodiscard]] inline LogRecord MakeLogRecord(LogLevel logLevel, std::string_view message)
	{
		return LogRecord{
			.logLevel = logLevel,
			.message = std::string(message),
		};
	}

	[[nodiscard]] inline LogRecord MakeLogRecord(
		time::SystemTimePoint timestamp,
		LogLevel logLevel,
		std::string_view message
	)
	{
		return LogRecord{
			.timestamp = timestamp,
			.logLevel = logLevel,
			.message = std::string(message),
		};
	}
}