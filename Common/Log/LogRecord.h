#pragma once

#include <string>
#include <string_view>

#include <Common/Log/LogLevel.h>
#include <Common/Time/TimeTypes.h>

namespace common::log
{
	// 로그 발생 시각과 심각도, 메시지를 출력 시점과 독립된 값으로 보관하는 구조체
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

	[[nodiscard]] inline LogRecord MakeLogRecord(time::SystemTimePoint timestamp, LogLevel logLevel, std::string_view message)
	{
		return LogRecord{
			.timestamp = timestamp,
			.logLevel = logLevel,
			.message = std::string(message),
		};
	}
}