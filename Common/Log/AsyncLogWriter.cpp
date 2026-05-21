#include "AsyncLogWriter.h"

#include <expected>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <Common/String/StringFormat.h>

namespace common::log
{
	AsyncLogWriter::~AsyncLogWriter() noexcept
	{
		Stop();
	}

	std::string AsyncLogWriter::ToString(const StartError& startError)
	{
		return std::visit(
			[](const auto& error) -> std::string
			{
				using ErrorType = std::remove_cvref_t<decltype(error)>;

				if constexpr (std::is_same_v<ErrorType, StartFailure>)
				{
					switch (error)
					{
					case StartFailure::InvalidWorkerThreadCount:
						return "InvalidWorkerThreadCount";

					case StartFailure::AlreadyStarted:
						return "AlreadyStarted";

					default:
						return "Unknown";
					}
				}
				else if constexpr (std::is_same_v<ErrorType, threading::ThreadPool::StartError>)
				{
					return common::string::FormatScopedName("ThreadPool", threading::ThreadPool::ToString(error));
				}
				else
				{
					return "Unknown";
				}
			},
			startError
		);
	}

	AsyncLogWriter::StartResult AsyncLogWriter::Start(std::size_t workerThreadCount)
	{
		if (workerThreadCount == 0)
		{
			return std::unexpected(StartError{ StartFailure::InvalidWorkerThreadCount });
		}

		if (isStarted_.load())
		{
			return std::unexpected(StartError{ StartFailure::AlreadyStarted });
		}

		const threading::ThreadPool::StartResult threadPoolStartResult = threadPool_.Start(workerThreadCount);
		if (!threadPoolStartResult.has_value())
		{
			return std::unexpected(StartError{ threadPoolStartResult.error() });
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