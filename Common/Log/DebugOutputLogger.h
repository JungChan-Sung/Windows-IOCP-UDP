#pragma once

#include <atomic>

#include <Common/Log/ILogger.h>
#include <Common/Log/LogLevel.h>
#include <Common/Log/LogRecord.h>

namespace common::log
{
	class DebugOutputLogger final : public ILogger
	{
	public:
		using ILogger::Log;

	private:
		std::atomic<LogLevel> minimumLogLevel_ = LogLevel::Info;

	public:
		DebugOutputLogger() = default;
		~DebugOutputLogger() noexcept override = default;

		DebugOutputLogger(const DebugOutputLogger&) = delete;
		DebugOutputLogger& operator=(const DebugOutputLogger&) = delete;

		DebugOutputLogger(DebugOutputLogger&&) = delete;
		DebugOutputLogger& operator=(DebugOutputLogger&&) = delete;

	public:
		bool Log(const LogRecord& logRecord) const override;

	public:
		void SetMinimumLogLevel(LogLevel logLevel) noexcept
		{
			minimumLogLevel_.store(logLevel);
		}

		[[nodiscard]] LogLevel GetMinimumLogLevel() const noexcept
		{
			return minimumLogLevel_.load();
		}

	private:
		[[nodiscard]] bool ShouldLog(LogLevel logLevel) const noexcept;
	};
}