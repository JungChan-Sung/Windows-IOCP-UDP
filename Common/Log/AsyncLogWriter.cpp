#include "AsyncLogWriter.h"

#include <utility>

namespace common::log
{
	AsyncLogWriter::~AsyncLogWriter() noexcept
	{
		Stop();
	}

	bool AsyncLogWriter::Start(std::size_t workerThreadCount)
	{
		if (workerThreadCount == 0)
		{
			return false;
		}

		if (isStarted_.load())
		{
			return false;
		}

		if (!threadPool_.Start(workerThreadCount))
		{
			return false;
		}

		isStarted_.store(true);
		return true;
	}

	void AsyncLogWriter::Stop() noexcept
	{
		if (!isStarted_.exchange(false))
		{
			return;
		}

		threadPool_.StopAfterDrain();
	}

	bool AsyncLogWriter::Log(LogLevel logLevel, std::string_view message) const
	{
		if (!isStarted_.load())
		{
			return false;
		}

		if (!ShouldEnqueue(logLevel))
		{
			return false;
		}

		std::string copiedMessage(message);

		return threadPool_.Enqueue(
			[this, logLevel, copiedMessage = std::move(copiedMessage)]()
			{
				logger_.Log(logLevel, copiedMessage);
			}
		);
	}

	bool AsyncLogWriter::ShouldEnqueue(LogLevel logLevel) const noexcept
	{
		return static_cast<int>(logLevel) >= static_cast<int>(logger_.GetMinimumLogLevel());
	}

	std::size_t AsyncLogWriter::GetPendingTaskCount() const
	{
		return threadPool_.GetPendingTaskCount();
	}
}