#include "DebugOutputLogger.h"

#include <Windows.h>

#include <string>

#include <Common/Log/LogFormatter.h>

namespace common::log
{
	bool DebugOutputLogger::Log(const LogRecord& logRecord) const
	{
		if (!ShouldLog(logRecord.logLevel))
		{
			return false;
		}

		std::string message = FormatLogMessage(logRecord);
		message += '\n';

		::OutputDebugStringA(message.c_str());

		return true;
	}

	bool DebugOutputLogger::ShouldLog(LogLevel logLevel) const noexcept
	{
		return static_cast<int>(logLevel) >= static_cast<int>(minimumLogLevel_.load());
	}
}