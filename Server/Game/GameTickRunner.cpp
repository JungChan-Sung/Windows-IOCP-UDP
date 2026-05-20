#include "GameTickRunner.h"

#include <expected>
#include <utility>

namespace server::game
{
	GameTickRunner::~GameTickRunner() noexcept
	{
		Stop();
	}

	std::string_view GameTickRunner::ToString(StartError startError) noexcept
	{
		switch (startError)
		{
		case StartError::AlreadyRunning:
			return "AlreadyRunning";

		case StartError::InvalidTickInterval:
			return "InvalidTickInterval";

		case StartError::InvalidTickHandler:
			return "InvalidTickHandler";

		case StartError::StartThreadFailed:
			return "StartThreadFailed";

		default:
			return "Unknown";
		}
	}

	GameTickRunner::StartResult GameTickRunner::Start(Duration tickInterval, TickHandler tickHandler)
	{
		if (isRunning_.load())
		{
			return std::unexpected(StartError::AlreadyRunning);
		}

		if (tickInterval <= Duration::zero())
		{
			return std::unexpected(StartError::InvalidTickInterval);
		}

		if (!tickHandler)
		{
			return std::unexpected(StartError::InvalidTickHandler);
		}

		tickHandler_ = std::move(tickHandler);
		isRunning_.store(true);

		try
		{
			tickThread_ = std::jthread(
				[this, tickInterval](std::stop_token stopToken)
				{
					RunLoop(stopToken, tickInterval);
				}
			);
		}
		catch (...)
		{
			isRunning_.store(false);
			tickHandler_ = nullptr;
			return std::unexpected(StartError::StartThreadFailed);
		}

		return {};
	}

	void GameTickRunner::Stop() noexcept
	{
		isRunning_.store(false);

		if (tickThread_.joinable())
		{
			tickThread_.request_stop();
		}

		tickThread_ = std::jthread();
		tickHandler_ = nullptr;
	}

	void GameTickRunner::RunLoop(std::stop_token stopToken, Duration tickInterval)
	{
		auto nextTickTime = Clock::now() + tickInterval;

		while (!stopToken.stop_requested())
		{
			std::this_thread::sleep_until(nextTickTime);

			if (stopToken.stop_requested())
			{
				break;
			}

			nextTickTime += tickInterval;

			if (tickHandler_)
			{
				tickHandler_();
			}

			const auto currentTime = Clock::now();
			if (nextTickTime < currentTime)
			{
				nextTickTime = currentTime + tickInterval;
			}
		}
	}
}