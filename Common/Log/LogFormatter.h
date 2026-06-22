#pragma once

#include <string>
#include <string_view>

#include <Common/Log/LogLevel.h>
#include <Common/Log/LogRecord.h>

namespace common::log
{
	[[nodiscard]] inline std::string FormatLogMessage(LogLevel logLevel, std::string_view message)
	{
		std::string result;
		result.reserve(ToString(logLevel).size() + message.size() + 4);

		result += '[';
		result += ToString(logLevel);
		result += "] ";
		result += message;

		return result;
	}

	[[nodiscard]] inline std::string FormatLogMessage(const LogRecord& logRecord)
	{
		return FormatLogMessage(logRecord.logLevel, logRecord.message);
	}
}