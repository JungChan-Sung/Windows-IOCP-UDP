#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <vector>

namespace common::threading
{
	class ThreadPool
	{
	public:
		using Task = std::function<void()>;

	private:
		using WorkerThreadList = std::vector<std::jthread>;
		using TaskQueue = std::queue<Task>;

	private:
		WorkerThreadList workerThreadList_;
		TaskQueue taskQueue_;

		mutable std::mutex taskMutex_;
		std::condition_variable_any taskCondition_;

		std::atomic<bool> isRunning_ = false;
		std::atomic<bool> isAcceptingTasks_ = false;

	public:
		ThreadPool() = default;
		~ThreadPool() noexcept;

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

		ThreadPool(ThreadPool&&) = delete;
		ThreadPool& operator=(ThreadPool&&) = delete;

	public:
		[[nodiscard]] bool Start(std::size_t workerThreadCount);
		void Stop() noexcept;
		void StopAfterDrain() noexcept;

		[[nodiscard]] bool Enqueue(Task task);

	private:
		void WorkerLoop(std::stop_token stopToken) noexcept;

	public:
		[[nodiscard]] bool IsRunning() const noexcept
		{
			return isRunning_.load();
		}

		[[nodiscard]] bool IsAcceptingTasks() const noexcept
		{
			return isAcceptingTasks_.load();
		}

		[[nodiscard]] std::size_t GetWorkerThreadCount() const noexcept
		{
			return workerThreadList_.size();
		}

		[[nodiscard]] std::size_t GetPendingTaskCount() const;
	};
}