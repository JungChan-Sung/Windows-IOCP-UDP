#pragma once

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include <Common/Log/LogLevel.h>
#include <Common/Log/LogRecord.h>
#include <Common/Time/TimeTypes.h>

namespace common::log
{
	[[nodiscard]] inline std::string FormatLogTimestamp(time::SystemTimePoint timestamp)
	{
		const auto millisecondTimestamp = std::chrono::time_point_cast<time::Milliseconds>(timestamp);

		const std::time_t timeValue = time::SystemClock::to_time_t(millisecondTimestamp);

		std::tm localTime{};
		if (::localtime_s(&localTime, &timeValue) != 0)
		{
			return "0000-00-00 00:00:00.000";
		}

		const time::Milliseconds milliseconds =
			std::chrono::duration_cast<time::Milliseconds>(
				millisecondTimestamp.time_since_epoch()
			) % time::Seconds(1);

		std::ostringstream stream;
		stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
			<< '.'
			<< std::setw(3)
			<< std::setfill('0')
			<< milliseconds.count();

		return stream.str();
	}

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
		const std::string timestampText = FormatLogTimestamp(logRecord.timestamp);

		std::string result;
		result.reserve(
			timestampText.size()
			+ ToString(logRecord.logLevel).size()
			+ logRecord.message.size()
			+ 6
		);

		result += '[';
		result += timestampText;
		result += "][";
		result += ToString(logRecord.logLevel);
		result += "] ";
		result += logRecord.message;

		return result;
	}
}