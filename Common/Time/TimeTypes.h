#pragma once

#include <chrono>

namespace common::time
{
	using SteadyClock = std::chrono::steady_clock;
	using Clock = SteadyClock;
	using TimePoint = Clock::time_point;
	using Duration = Clock::duration;

	using SystemClock = std::chrono::system_clock;
	using SystemTimePoint = SystemClock::time_point;
	using SystemDuration = SystemClock::duration;

	using Milliseconds = std::chrono::milliseconds;
	using Seconds = std::chrono::seconds;
	using FloatSeconds = std::chrono::duration<float>;
}