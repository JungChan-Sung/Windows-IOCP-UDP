#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <string_view>

#include <Common/Threading/ThreadPool.h>

#include "ConsoleLogger.h"
#include "LogLevel.h"

namespace common::log
{
	class AsyncLogWriter
	{
	private:
		ConsoleLogger logger_;
		threading::ThreadPool threadPool_;

		std::atomic<bool> isStarted_ = false;

	public:
		AsyncLogWriter() = default;
		~AsyncLogWriter() noexcept;

		AsyncLogWriter(const AsyncLogWriter&) = delete;
		AsyncLogWriter& operator=(const AsyncLogWriter&) = delete;

		AsyncLogWriter(AsyncLogWriter&&) = delete;
		AsyncLogWriter& operator=(AsyncLogWriter&&) = delete;

	public:
		[[nodiscard]] bool Start(std::size_t workerThreadCount = 1);
		void Stop() noexcept;

		[[nodiscard]] bool Log(LogLevel logLevel, std::string_view message);
		[[nodiscard]] bool Trace(std::string_view message);
		[[nodiscard]] bool Debug(std::string_view message);
		[[nodiscard]] bool Info(std::string_view message);
		[[nodiscard]] bool Warning(std::string_view message);
		[[nodiscard]] bool Error(std::string_view message);

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