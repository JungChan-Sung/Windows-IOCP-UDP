#include "SystemDateTime.h"

#include <chrono>

namespace common::time
{
	SystemDateTime ToSystemDateTime(SystemTimePoint timePoint) noexcept
	{
		const auto milliseconds = std::chrono::floor<std::chrono::milliseconds>(timePoint);
		const auto dayPoint = std::chrono::floor<std::chrono::days>(milliseconds);

		const std::chrono::year_month_day date{ dayPoint };
		const std::chrono::hh_mm_ss timeOfDay{ milliseconds - dayPoint };

		return SystemDateTime{
			.year = static_cast<int>(date.year()),
			.month = static_cast<std::uint32_t>(static_cast<unsigned int>(date.month())),
			.day = static_cast<std::uint32_t>(static_cast<unsigned int>(date.day())),
			.hour = static_cast<std::uint32_t>(timeOfDay.hours().count()),
			.minute = static_cast<std::uint32_t>(timeOfDay.minutes().count()),
			.second = static_cast<std::uint32_t>(timeOfDay.seconds().count()),
			.millisecond = static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(timeOfDay.subseconds()).count()),
		};
	}
}