#pragma once

#include <cstdint>

namespace common::game
{
	enum class EffectType : std::uint8_t
	{
		None = 0,
		Impact = 1,
		Spawn = 2,
	};
}