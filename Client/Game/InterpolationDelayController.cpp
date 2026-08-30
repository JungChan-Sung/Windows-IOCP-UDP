#include "InterpolationDelayController.h"

#include <algorithm>
#include <chrono>

#include <Common/Game/SimulationConstants.h>

#include <Client/Game/ClientTuning.h>

namespace client::game
{
	InterpolationDelayController::InterpolationDelayController()
		: defaultDelay_(game::defaultInterpolationDelay),
		minDelay_(game::minInterpolationDelay),
		maxDelay_(game::maxInterpolationDelay),
		delay_(game::defaultInterpolationDelay)
	{}

	InterpolationDelayController::Milliseconds InterpolationDelayController::CalculateRequiredDelay(TimePoint arrivalTime, TimePoint sampleTime, Milliseconds tickInterval) noexcept
	{
		const Milliseconds resolvedTickInterval = (tickInterval > Milliseconds::zero()) ? tickInterval : common::game::defaultFixedTickInterval;
		if (arrivalTime <= sampleTime)
		{
			return resolvedTickInterval;
		}

		const Milliseconds arrivalLag = std::chrono::ceil<Milliseconds>(arrivalTime - sampleTime);

		return arrivalLag + resolvedTickInterval;
	}

	void InterpolationDelayController::Reset() noexcept
	{
		delay_ = defaultDelay_;
	}

	void InterpolationDelayController::Configure(Milliseconds defaultDelay, Milliseconds minDelay, Milliseconds maxDelay) noexcept
	{
		minDelay_ = std::max(minDelay, Milliseconds::zero());
		maxDelay_ = std::max(maxDelay, minDelay_);
		defaultDelay_ = std::clamp(defaultDelay, minDelay_, maxDelay_);
		delay_ = defaultDelay_;
	}

	void InterpolationDelayController::ObserveSnapshotTiming(TimePoint arrivalTime, TimePoint sampleTime, Milliseconds tickInterval) noexcept
	{
		const Milliseconds requiredDelay = CalculateRequiredDelay(arrivalTime, sampleTime, tickInterval);
		if (requiredDelay <= delay_)
		{
			return;
		}

		SetDelay(requiredDelay);
	}

	void InterpolationDelayController::SetDelay(Milliseconds delay) noexcept
	{
		delay_ = std::clamp(delay, minDelay_, maxDelay_);
	}
}