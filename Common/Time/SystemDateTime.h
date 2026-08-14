#pragma once

#include <cstdint>

#include <Common/Time/TimeTypes.h>

namespace common::time
{
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