#include "ServerTickTimeline.h"

#include <chrono>
#include <cstdint>

#include <Common/Game/SimulationConstants.h>

namespace client::game
{
	ServerTickTimeline::ServerTickTimeline()
		: tickInterval_(common::game::defaultFixedTickInterval),
		anchorTime_()
	{}

	ServerTickTimeline::Milliseconds ServerTickTimeline::ResolveTickInterval(Milliseconds tickInterval) noexcept
	{
		if (tickInterval > Milliseconds::zero())
		{
			return tickInterval;
		}

		return common::game::defaultFixedTickInterval;
	}

	void ServerTickTimeline::Clear() noexcept
	{
		tickInterval_ = common::game::defaultFixedTickInterval;
		anchorTime_ = TimePoint();

		anchorServerTick_ = 0;
		isInitialized_ = false;
	}

	ServerTickTimeline::TimePoint ServerTickTimeline::ResolveSampleTime(std::uint32_t serverTick, Milliseconds tickInterval, TimePoint arrivalTime) noexcept
	{
		const Milliseconds resolvedTickInterval = ResolveTickInterval(tickInterval);

		if (!isInitialized_ || resolvedTickInterval != tickInterval_)
		{
			tickInterval_ = resolvedTickInterval;
			anchorTime_ = arrivalTime;

			anchorServerTick_ = serverTick;
			isInitialized_ = true;

			return arrivalTime;
		}

		const std::uint32_t tickOffset = serverTick - anchorServerTick_;
		const common::time::Duration elapsedTime = std::chrono::duration_cast<common::time::Duration>(
				tickInterval_ * static_cast<std::int64_t>(tickOffset)
			);

		return anchorTime_ + elapsedTime;
	}
}