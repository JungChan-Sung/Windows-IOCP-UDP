#pragma once

#include <atomic>
#include <expected>
#include <functional>
#include <stop_token>
#include <string_view>
#include <thread>

#include <Common/Time/TimeTypes.h>

namespace server::game
{
	class GameTickRunner
	{
	public:
		enum class StartError
		{
			AlreadyRunning,
			InvalidTickInterval,
			InvalidTickHandler,
			StartThreadFailed,
		};

	public:
		using StartResult = std::expected<void, StartError>;

		using Clock = common::time::Clock;
		using Duration = common::time::Duration;
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
		[[nodiscard]] static std::string_view ToString(StartError startError) noexcept;

	public:
		[[nodiscard]] StartResult Start(Duration tickInterval, TickHandler tickHandler);
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

