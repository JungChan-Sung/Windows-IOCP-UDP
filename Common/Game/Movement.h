#pragma once

#include <cmath>

#include "InputFlags.h"

namespace common::game
{
	struct MoveDirection
	{
	public:
		float x = 0.0F;
		float y = 0.0F;
	};

	[[nodiscard]] inline MoveDirection BuildNormalizedMoveDirection(InputFlags inputFlags) noexcept
	{
		MoveDirection direction{};

		if (HasFlag(inputFlags, InputFlags::Left))
		{
			direction.x -= 1.0F;
		}

		if (HasFlag(inputFlags, InputFlags::Right))
		{
			direction.x += 1.0F;
		}

		if (HasFlag(inputFlags, InputFlags::Up))
		{
			direction.y -= 1.0F;
		}

		if (HasFlag(inputFlags, InputFlags::Down))
		{
			direction.y += 1.0F;
		}

		const float lengthSquared = (direction.x * direction.x) + (direction.y * direction.y);
		if (lengthSquared <= 0.0F)
		{
			return direction;
		}

		const float length = std::sqrt(lengthSquared);
		direction.x /= length;
		direction.y /= length;

		return direction;
	}

	inline void ApplyMovement(
		float& x,
		float& y,
		InputFlags inputFlags,
		float deltaSeconds,
		float moveSpeed
	) noexcept
	{
		const MoveDirection direction = BuildNormalizedMoveDirection(inputFlags);
		const float moveDistance = moveSpeed * deltaSeconds;

		x += direction.x * moveDistance;
		y += direction.y * moveDistance;
	}
}