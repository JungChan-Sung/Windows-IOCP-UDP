#include "ConsoleLogger.h"

#include <iostream>

#include <Common/Log/LogFormatter.h>

namespace common::log
{
	bool ConsoleLogger::Log(LogLevel logLevel, std::string_view message) const
	{
		return Log(MakeLogRecord(logLevel, message));
	}

	bool ConsoleLogger::Log(const LogRecord& logRecord) const
	{
		if (!ShouldLog(logRecord.logLevel))
		{
			return false;
		}

		std::scoped_lock lock(logMutex_);

		std::ostream& outputStream = (logRecord.logLevel >= LogLevel::Warning) ? std::cerr : std::cout;
		outputStream << FormatLogMessage(logRecord) << '\n';

		return true;
	}

	bool ConsoleLogger::ShouldLog(LogLevel logLevel) const noexcept
	{
		return static_cast<int>(logLevel) >= static_cast<int>(minimumLogLevel_.load());
	}
}