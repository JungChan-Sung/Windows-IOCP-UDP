#pragma once

#include <cstddef>

namespace persistence::config
{
	inline constexpr std::size_t maxServerConfigKeyUtf16CodeUnitCount = 128;
	inline constexpr std::size_t maxServerConfigValueUtf16CodeUnitCount = 512;
}