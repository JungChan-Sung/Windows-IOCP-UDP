#include "AsyncLogWriter.h"

#include <expected>
#include <string>
#include <utility>

namespace common::log
{
	AsyncLogWriter::~AsyncLogWriter() noexcept
	{
		Stop();
	}

	std::string_view AsyncLogWriter::ToString(StartError startError) noexcept
	{
		switch (startError)
		{
		case StartError::InvalidWorkerThreadCount:
			return "InvalidWorkerThreadCount";

		case StartError::AlreadyStarted:
			return "AlreadyStarted";

		case StartError::ThreadPoolInvalidWorkerThreadCount:
			return "ThreadPoolInvalidWorkerThreadCount";

		case StartError::ThreadPoolAlreadyRunning:
			return "ThreadPoolAlreadyRunning";

		case StartError::ThreadPoolStartWorkerThreadsFailed:
			return "ThreadPoolStartWorkerThreadsFailed";

		default:
			return "Unknown";
		}
	}

	AsyncLogWriter::StartError AsyncLogWriter::ToStartError(threading::ThreadPool::StartError startError) noexcept
	{
		switch (startError)
		{
		case threading::ThreadPool::StartError::InvalidWorkerThreadCount:
			return StartError::ThreadPoolInvalidWorkerThreadCount;

		case threading::ThreadPool::StartError::AlreadyRunning:
			return StartError::ThreadPoolAlreadyRunning;

		case threading::ThreadPool::StartError::StartWorkerThreadsFailed:
			return StartError::ThreadPoolStartWorkerThreadsFailed;

		default:
			return StartError::ThreadPoolStartWorkerThreadsFailed;
		}
	}

	AsyncLogWriter::StartResult AsyncLogWriter::Start(std::size_t workerThreadCount)
	{
		if (workerThreadCount == 0)
		{
			return std::unexpected(StartError::InvalidWorkerThreadCount);
		}

		if (isStarted_.load())
		{
			return std::unexpected(StartError::AlreadyStarted);
		}

		const threading::ThreadPool::StartResult threadPoolStartResult = threadPool_.Start(workerThreadCount);
		if (!threadPoolStartResult.has_value())
		{
			return std::unexpected(ToStartError(threadPoolStartResult.error()));
		}

		isStarted_.store(true);
		return {};
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