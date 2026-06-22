#pragma once

#include <string_view>

#include <Common/Log/LogLevel.h>
#include <Common/Log/LogRecord.h>

namespace common::log
{
	class ILogger
	{
	public:
		ILogger() = default;
		virtual ~ILogger() noexcept = default;

		ILogger(const ILogger&) = delete;
		ILogger& operator=(const ILogger&) = delete;

		ILogger(ILogger&&) = delete;
		ILogger& operator=(ILogger&&) = delete;

	public:
		virtual bool Log(const LogRecord& logRecord) const = 0;

		bool Log(LogLevel logLevel, std::string_view message) const
		{
			return Log(MakeLogRecord(logLevel, message));
		}

		bool Trace(std::string_view message) const
		{
			return Log(LogLevel::Trace, message);
		}
		bool Debug(std::string_view message) const
		{
			return Log(LogLevel::Debug, message);
		}
		bool Info(std::string_view message) const
		{
			return Log(LogLevel::Info, message);
		}
		bool Warning(std::string_view message) const
		{
			return Log(LogLevel::Warning, message);
		}
		bool Error(std::string_view message) const
		{
			return Log(LogLevel::Error, message);
		}
	};
}