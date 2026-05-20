#include "ThreadPool.h"

#include <expected>
#include <utility>

namespace common::threading
{
	ThreadPool::~ThreadPool() noexcept
	{
		Stop();
	}

	std::string_view ThreadPool::ToString(StartError startError) noexcept
	{
		switch (startError)
		{
		case StartError::InvalidWorkerThreadCount:
			return "InvalidWorkerThreadCount";

		case StartError::AlreadyRunning:
			return "AlreadyRunning";

		case StartError::StartWorkerThreadsFailed:
			return "StartWorkerThreadsFailed";

		default:
			return "Unknown";
		}
	}

	ThreadPool::StartResult ThreadPool::Start(std::size_t workerThreadCount)
	{
		if (workerThreadCount == 0)
		{
			return std::unexpected(StartError::InvalidWorkerThreadCount);
		}

		if (isRunning_.exchange(true))
		{
			return std::unexpected(StartError::AlreadyRunning);
		}

		isAcceptingTasks_.store(true);

		try
		{
			workerThreadList_.reserve(workerThreadCount);

			for (std::size_t index = 0; index < workerThreadCount; ++index)
			{
				workerThreadList_.emplace_back(
					[this](std::stop_token stopToken)
					{
						WorkerLoop(stopToken);
					}
				);
			}
		}
		catch (...)
		{
			Stop();
			return std::unexpected(StartError::StartWorkerThreadsFailed);
		}

		return {};
	}

	void ThreadPool::Stop() noexcept
	{
		if (!isRunning_.exchange(false))
		{
			return;
		}

		isAcceptingTasks_.store(false);

		for (std::jthread& workerThread : workerThreadList_)
		{
			workerThread.request_stop();
		}

		taskCondition_.notify_all();
		workerThreadList_.clear();

		{
			std::scoped_lock lock(taskMutex_);

			TaskQueue emptyTaskQueue;
			taskQueue_.swap(emptyTaskQueue);
		}
	}

	void ThreadPool::StopAfterDrain() noexcept
	{
		if (!isRunning_.exchange(false))
		{
			return;
		}

		isAcceptingTasks_.store(false);
		taskCondition_.notify_all();

		for (std::jthread& workerThread : workerThreadList_)
		{
			if (workerThread.joinable())
			{
				workerThread.join();
			}
		}

		workerThreadList_.clear();

		{
			std::scoped_lock lock(taskMutex_);

			TaskQueue emptyTaskQueue;
			taskQueue_.swap(emptyTaskQueue);
		}
	}

	bool ThreadPool::Enqueue(Task task)
	{
		if (!task)
		{
			return false;
		}

		{
			std::scoped_lock lock(taskMutex_);

			if (!isAcceptingTasks_.load())
			{
				return false;
			}

			taskQueue_.push(std::move(task));
		}

		taskCondition_.notify_one();
		return true;
	}

	void ThreadPool::WorkerLoop(std::stop_token stopToken) noexcept
	{
		while (!stopToken.stop_requested())
		{
			Task task;

			{
				std::unique_lock lock(taskMutex_);

				taskCondition_.wait(
					lock,
					stopToken,
					[this]()
					{
						return !taskQueue_.empty() || !isRunning_.load();
					}
				);

				if (stopToken.stop_requested())
				{
					return;
				}

				if (taskQueue_.empty())
				{
					if (!isRunning_.load())
					{
						return;
					}

					continue;
				}

				task = std::move(taskQueue_.front());
				taskQueue_.pop();
			}

			try
			{
				task();
			}
			catch (...)
			{
			}
		}
	}

	std::size_t ThreadPool::GetPendingTaskCount() const
	{
		std::scoped_lock lock(taskMutex_);
		return taskQueue_.size();
	}
}