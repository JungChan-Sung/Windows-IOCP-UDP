#pragma once

#include <atomic>
#include <mutex>
#include <string_view>

#include <Common/Log/ILogger.h>
#include <Common/Log/LogLevel.h>

namespace common::log
{
	class ConsoleLogger final : public ILogger
	{
	private:
		mutable std::mutex logMutex_;
		std::atomic<LogLevel> minimumLogLevel_ = LogLevel::Info;

	public:
		ConsoleLogger() = default;
		~ConsoleLogger() noexcept override = default;

		ConsoleLogger(const ConsoleLogger&) = delete;
		ConsoleLogger& operator=(const ConsoleLogger&) = delete;

		ConsoleLogger(ConsoleLogger&&) = delete;
		ConsoleLogger& operator=(ConsoleLogger&&) = delete;

	public:
		bool Log(LogLevel logLevel, std::string_view message) const override;

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