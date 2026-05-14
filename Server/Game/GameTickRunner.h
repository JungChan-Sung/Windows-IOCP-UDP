#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <stop_token>
#include <thread>

namespace server::game
{
	class GameTickRunner
	{
	public:
		using Clock = std::chrono::steady_clock;
		using Duration = Clock::duration;
		using TickHandler = std::function<void()>;

	private:
		std::jthread tickThread_;
		std::atomic<bool> isRunning_ = false;
		TickHandler tickHandler_;

	public:
		GameTickRunner() = default;
		~GameTickRunner() noexcept;

		GameTickRunner(const GameTickRunner&) = delete;
		GameTickRunner& operator=(const GameTickRunner&) = delete;

		GameTickRunner(GameTickRunner&&) = delete;
		GameTickRunner& operator=(GameTickRunner&&) = delete;

	public:
		[[nodiscard]] bool Start(Duration tickInterval, TickHandler tickHandler);
		void Stop() noexcept;

	private:
		void RunLoop(std::stop_token stopToken, Duration tickInterval);

	public:
		[[nodiscard]] bool IsRunning() const noexcept
		{
			return isRunning_.load();
		}
	};
}

