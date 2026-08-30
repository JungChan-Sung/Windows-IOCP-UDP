#pragma once

#include <Common/Time/TimeTypes.h>

namespace client::game
{
	class InterpolationDelayController
	{
	public:
		using TimePoint = common::time::TimePoint;
		using Milliseconds = common::time::Milliseconds;

	private:
		Milliseconds defaultDelay_;
		Milliseconds minDelay_;
		Milliseconds maxDelay_;
		Milliseconds delay_;

	public:
		InterpolationDelayController();
		~InterpolationDelayController() noexcept = default;

		InterpolationDelayController(const InterpolationDelayController&) = delete;
		InterpolationDelayController& operator=(const InterpolationDelayController&) = delete;

		InterpolationDelayController(InterpolationDelayController&&) = delete;
		InterpolationDelayController& operator=(InterpolationDelayController&&) = delete;

	private:
		[[nodiscard]] static Milliseconds CalculateRequiredDelay(TimePoint arrivalTime, TimePoint sampleTime, Milliseconds tickInterval) noexcept;

	public:
		void Reset() noexcept;
		void Configure(Milliseconds defaultDelay, Milliseconds minDelay, Milliseconds maxDelay) noexcept;
		void ObserveSnapshotTiming(TimePoint arrivalTime, TimePoint sampleTime, Milliseconds tickInterval) noexcept;

	public:
		void SetDelay(Milliseconds delay) noexcept;

		[[nodiscard]] Milliseconds GetDelay() const noexcept
		{
			return delay_;
		}

		[[nodiscard]] Milliseconds GetMaxDelay() const noexcept
		{
			return maxDelay_;
		}
	};
}