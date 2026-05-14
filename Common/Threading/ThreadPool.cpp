#include "ThreadPool.h"

#include <utility>

namespace common::threading
{
	ThreadPool::~ThreadPool() noexcept
	{
		Stop();
	}

	bool ThreadPool::Start(std::size_t workerThreadCount)
	{
		if (workerThreadCount == 0)
		{
			return false;
		}

		if (isRunning_.exchange(true))
		{
			return false;
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
			return false;
		}

		return true;
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