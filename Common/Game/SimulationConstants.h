#pragma once

#include <chrono>

namespace common::game
{
	inline constexpr std::chrono::milliseconds defaultFixedTickInterval = std::chrono::milliseconds(50);
	inline constexpr float defaultFixedDeltaSeconds = 0.05F;

	inline constexpr float defaultMoveSpeed = 220.0F;
	inline constexpr float playerHalfExtent = 10.0F;

	struct WorldBounds
	{
	public:
		float minX = 0.0F;
		float minY = 0.0F;
		float maxX = 1280.0F;
		float maxY = 720.0F;
	};

	inline constexpr WorldBounds defaultWorldBounds{};
}