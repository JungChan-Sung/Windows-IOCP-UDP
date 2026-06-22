#pragma once

#include <string>
#include <string_view>

#include <Common/Log/LogLevel.h>

namespace common::log
{
	struct LogRecord
	{
	public:
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
}