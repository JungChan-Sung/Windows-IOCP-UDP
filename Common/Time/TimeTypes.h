#pragma once

#include <chrono>

namespace common::time
{
	// Timeout과 경과 시간 계산에는 시스템 시각 변경의 영향을 받지 않는 Steady Clock을 사용
	using SteadyClock = std::chrono::steady_clock;
	using Clock = SteadyClock;
	using TimePoint = Clock::time_point;
	using Duration = Clock::duration;

	// 로그나 영속성 데이터처럼 실제 시각이 필요한 경우에는 System Clock을 사용
	using SystemClock = std::chrono::system_clock;
	using SystemTimePoint = SystemClock::time_point;
	using SystemDuration = SystemClock::duration;

	using Milliseconds = std::chrono::milliseconds;
	using Seconds = std::chrono::seconds;
	using FloatSeconds = std::chrono::duration<float>;
}