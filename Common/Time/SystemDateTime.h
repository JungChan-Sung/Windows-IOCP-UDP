#pragma once

#include <cstdint>

#include <Common/Time/TimeTypes.h>

namespace common::time
{
	// System Clock 시각을 저장·출력하기 쉬운 날짜와 시간 구성 요소로 표현한 구조체
	struct SystemDateTime
	{
	public:
		int year = 0;
		std::uint32_t month = 0;
		std::uint32_t day = 0;

		std::uint32_t hour = 0;
		std::uint32_t minute = 0;
		std::uint32_t second = 0;
		std::uint32_t millisecond = 0;
	};

	[[nodiscard]] SystemDateTime ToSystemDateTime(SystemTimePoint timePoint) noexcept;
}