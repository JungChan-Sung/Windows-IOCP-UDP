#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <expected>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <string_view>
#include <thread>
#include <vector>

namespace common::threading
{
	// 고정 Worker Thread가 공유 작업 Queue를 처리하는 클래스
	// 즉시 종료와 Pending 작업을 소진한 뒤 종료하는 두 정책을 지원
	class ThreadPool
	{
	public:
		enum class StartError
		{
			InvalidWorkerThreadCount,
			AlreadyRunning,
			StartWorkerThreadsFailed,
		};

	public:
		using StartResult = std::expected<void, StartError>;
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
		[[nodiscard]] static std::string_view ToString(StartError startError) noexcept;

	public:
		[[nodiscard]] StartResult Start(std::size_t workerThreadCount);
		// Worker에 Stop을 요청하고 아직 실행되지 않은 Pending 작업은 폐기하는 함수
		void Stop() noexcept;
		// 새 작업 접수를 중단하고 이미 Queue에 들어온 작업을 모두 처리한 뒤 Worker를 종료하는 함수
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