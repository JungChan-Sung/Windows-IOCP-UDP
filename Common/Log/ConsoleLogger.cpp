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
		outputStream << '[' << ToString(logLevel) << "] " << message << '\n';

		return true;
	}

	bool ConsoleLogger::ShouldLog(LogLevel logLevel) const noexcept
	{
		return static_cast<int>(logLevel) >= static_cast<int>(minimumLogLevel_.load());
	}

	std::string_view ConsoleLogger::ToString(LogLevel logLevel) noexcept
	{
		switch (logLevel)
		{
		case LogLevel::Trace:
			return "Trace";

		case LogLevel::Debug:
			return "Debug";

		case LogLevel::Info:
			return "Info";

		case LogLevel::Warning:
			return "Warning";

		case LogLevel::Error:
			return "Error";

		default:
			return "Unknown";
		}
	}
}