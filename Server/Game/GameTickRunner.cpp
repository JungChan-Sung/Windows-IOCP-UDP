#include "GameTickRunner.h"

#include <utility>

namespace server::game
{
	GameTickRunner::~GameTickRunner() noexcept
	{
		Stop();
	}

	bool GameTickRunner::Start(Duration tickInterval, TickHandler tickHandler)
	{
		if (isRunning_.load())
		{
			return false;
		}

		if (tickInterval <= Duration::zero())
		{
			return false;
		}

		if (!tickHandler)
		{
			return false;
		}

		tickHandler_ = std::move(tickHandler);
		isRunning_.store(true);

		tickThread_ = std::jthread(
			[this, tickInterval](std::stop_token stopToken)
			{
				RunLoop(stopToken, tickInterval);
			}
		);

		return true;
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