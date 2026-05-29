#pragma once

#include <chrono>

namespace common::time
{
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;
	using Duration = Clock::duration;
	using Milliseconds = std::chrono::milliseconds;
}