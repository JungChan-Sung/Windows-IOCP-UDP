#pragma once

#include <atomic>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <variant>

#include <Common/Log/ConsoleLogger.h>
#include <Common/Log/ILogger.h>
#include <Common/Log/LogLevel.h>
#include <Common/Threading/ThreadPool.h>

namespace common::log
{
	class AsyncLogWriter final : public ILogger
	{
	public:
		using ILogger::Log;

	public:
		enum class StartFailure
		{
			InvalidWorkerThreadCount,
			AlreadyStarted,
		};

	public:
		using StartError = std::variant<StartFailure, threading::ThreadPool::StartError>;
		using StartResult = std::expected<void, StartError>;

	private:
		ConsoleLogger defaultLogger_;
		const ILogger* logger_ = &defaultLogger_;

		mutable threading::ThreadPool threadPool_;
		mutable std::atomic<bool> isStarted_ = false;

		std::atomic<LogLevel> minimumLogLevel_ = LogLevel::Info;

	public:
		AsyncLogWriter() = default;
		~AsyncLogWriter() noexcept override;

		AsyncLogWriter(const AsyncLogWriter&) = delete;
		AsyncLogWriter& operator=(const AsyncLogWriter&) = delete;

		AsyncLogWriter(AsyncLogWriter&&) = delete;
		AsyncLogWriter& operator=(AsyncLogWriter&&) = delete;

	public:
		[[nodiscard]] static std::string ToString(const StartError& startError);

	public:
		[[nodiscard]] StartResult Start(std::size_t workerThreadCount = 1);
		void Stop() noexcept;

		bool Log(const LogRecord& logRecord) const override;

	private:
		[[nodiscard]] bool ShouldEnqueue(LogLevel logLevel) const noexcept;

	public:
		void SetLogger(const ILogger& logger) noexcept
		{
			logger_ = &logger;
		}

		void ResetLogger() noexcept
		{
			logger_ = &defaultLogger_;
		}

		void SetMinimumLogLevel(LogLevel logLevel) noexcept
		{
			minimumLogLevel_.store(logLevel);
			defaultLogger_.SetMinimumLogLevel(logLevel);
		}

		[[nodiscard]] LogLevel GetMinimumLogLevel() const noexcept
		{
			return minimumLogLevel_.load();
		}

		[[nodiscard]] bool IsStarted() const noexcept
		{
			return isStarted_.load();
		}

		[[nodiscard]] std::size_t GetPendingTaskCount() const;
	};
}