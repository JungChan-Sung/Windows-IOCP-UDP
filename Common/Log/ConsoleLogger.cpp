#include "ConsoleLogger.h"

#include <iostream>

namespace common::log
{
	bool ConsoleLogger::Log(LogLevel logLevel, std::string_view message) const
	{
		if (!ShouldLog(logLevel))
		{
			return false;
		}

		std::scoped_lock lock(logMutex_);

		std::ostream& outputStream = (logLevel >= LogLevel::Warning) ? std::cerr : std::cout;
		outputStream << '[' << common::log::ToString(logLevel) << "] " << message << '\n';

		return true;
	}

	bool ConsoleLogger::ShouldLog(LogLevel logLevel) const noexcept
	{
		return static_cast<int>(logLevel) >= static_cast<int>(minimumLogLevel_.load());
	}
}