#pragma once
#pragma once

#include <string>
#include <string_view>

#include <Common/Log/LogLevel.h>

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
}