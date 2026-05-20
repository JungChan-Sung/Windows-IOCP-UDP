#pragma once

#include <atomic>
#include <cstddef>
#include <expected>
#include <string_view>

#include <Common/Log/ConsoleLogger.h>
#include <Common/Log/ILogger.h>
#include <Common/Log/LogLevel.h>
#include <Common/Threading/ThreadPool.h>

namespace common::log
{
	class AsyncLogWriter final : public ILogger
	{
	public:
		enum class StartError
		{
			InvalidWorkerThreadCount,
			AlreadyStarted,

			ThreadPoolInvalidWorkerThreadCount,
			ThreadPoolAlreadyRunning,
			ThreadPoolStartWorkerThreadsFailed,
		};

	public:
		using StartResult = std::expected<void, StartError>;

	private:
		ConsoleLogger logger_;
		mutable threading::ThreadPool threadPool_;

		mutable std::atomic<bool> isStarted_ = false;

	public:
		AsyncLogWriter() = default;
		~AsyncLogWriter() noexcept override;

		AsyncLogWriter(const AsyncLogWriter&) = delete;
		AsyncLogWriter& operator=(const AsyncLogWriter&) = delete;

		AsyncLogWriter(AsyncLogWriter&&) = delete;
		AsyncLogWriter& operator=(AsyncLogWriter&&) = delete;

	public:
		[[nodiscard]] static std::string_view ToString(StartError startError) noexcept;

	private:
		[[nodiscard]] static StartError ToStartError(threading::ThreadPool::StartError startError) noexcept;

	public:
		[[nodiscard]] StartResult Start(std::size_t workerThreadCount = 1);
		void Stop() noexcept;

		bool Log(LogLevel logLevel, std::string_view message) const override;

	private:
		[[nodiscard]] bool ShouldEnqueue(LogLevel logLevel) const noexcept;

	public:
		void SetMinimumLogLevel(LogLevel logLevel) noexcept
		{
			logger_.SetMinimumLogLevel(logLevel);
		}

		[[nodiscard]] LogLevel GetMinimumLogLevel() const noexcept
		{
			return logger_.GetMinimumLogLevel();
		}

		[[nodiscard]] bool IsStarted() const noexcept
		{
			return isStarted_.load();
		}

		[[nodiscard]] std::size_t GetPendingTaskCount() const;
	};
}