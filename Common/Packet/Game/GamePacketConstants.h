#pragma once

#include <cstddef>

namespace common::packet
{
	inline constexpr std::size_t maxPlayersPerSnapshot = 16;
	inline constexpr std::size_t maxBulletsPerSnapshot = 32;
	inline constexpr std::size_t maxImpactEffectsPerPacket = 16;
}