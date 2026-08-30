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
		requestedDelay_(game::defaultInterpolationDelay),
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
		requestedDelay_ = defaultDelay_;
		delay_ = defaultDelay_;

		isDecreaseScheduled_ = false;
	}

	void InterpolationDelayController::Configure(Milliseconds defaultDelay, Milliseconds minDelay, Milliseconds maxDelay) noexcept
	{
		minDelay_ = std::max(minDelay, Milliseconds::zero());
		maxDelay_ = std::max(maxDelay, minDelay_);
		defaultDelay_ = std::clamp(defaultDelay, minDelay_, maxDelay_);

		requestedDelay_ = defaultDelay_;
		delay_ = defaultDelay_;

		isDecreaseScheduled_ = false;
	}

	void InterpolationDelayController::ObserveSnapshotTiming(TimePoint arrivalTime, TimePoint sampleTime, Milliseconds tickInterval) noexcept
	{
		const Milliseconds requiredDelay = CalculateRequiredDelay(arrivalTime, sampleTime, tickInterval);
		const Milliseconds targetDelay = std::max(requestedDelay_, requiredDelay);
		if (targetDelay >= delay_)
		{
			delay_ = std::min(targetDelay, maxDelay_);
			isDecreaseScheduled_ = false;
			return;
		}

		if (!isDecreaseScheduled_)
		{
			nextDecreaseTime_ = arrivalTime + automaticDecreaseHoldDuration;
			isDecreaseScheduled_ = true;
			return;
		}

		if (arrivalTime < nextDecreaseTime_)
		{
			return;
		}

		delay_ = std::max(targetDelay, delay_ - automaticDecreaseStep);
		if (delay_ <= targetDelay)
		{
			isDecreaseScheduled_ = false;
			return;
		}

		nextDecreaseTime_ = arrivalTime + automaticDecreaseInterval;
	}
}