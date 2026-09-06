#pragma once

#include <algorithm>
#include <cmath>
#include <span>

#include <Common/Packet/PacketType.h>

#include "Movement.h"
#include "SimulationConstants.h"

namespace common::game
{
	// (minX, minY)
	//	    ┌───────┐
	//	    │              │
	//	    │     Wall     │
	//   	│              │
	//	    └───────┘
	//              	(maxX, maxY)
	struct WallRect
	{
	public:
		float minX = 0.0F;
		float minY = 0.0F;
		float maxX = 0.0F;
		float maxY = 0.0F;
	};

	inline void ClampPositionToWorldBounds(
		float& x,
		float& y,
		float halfExtent,
		const WorldBounds& worldBounds
	) noexcept
	{
		x = std::clamp(x, worldBounds.minX + halfExtent, worldBounds.maxX - halfExtent);
		y = std::clamp(y, worldBounds.minY + halfExtent, worldBounds.maxY - halfExtent);
	}

	[[nodiscard]] inline bool IsAabbOverlappingWall(
		float minX,
		float minY,
		float maxX,
		float maxY,
		const WallRect& wallRect
	) noexcept
	{
		return minX < wallRect.maxX
			&& maxX > wallRect.minX
			&& minY < wallRect.maxY
			&& maxY > wallRect.minY;
	}

	[[nodiscard]] inline bool IsPlayerCollidingWithWall(
		float x,
		float y,
		float halfExtent,
		const WallRect& wallRect
	) noexcept
	{
		return IsAabbOverlappingWall(
			x - halfExtent,
			y - halfExtent,
			x + halfExtent,
			y + halfExtent,
			wallRect
		);
	}

	[[nodiscard]] inline bool IsPlayerCollidingWithAnyWall(
		float x,
		float y,
		float halfExtent,
		std::span<const WallRect> wallRectList
	) noexcept
	{
		for (const WallRect& wallRect : wallRectList)
		{
			if (IsPlayerCollidingWithWall(x, y, halfExtent, wallRect))
			{
				return true;
			}
		}

		return false;
	}

	[[nodiscard]] inline bool IsRangeOverlapping(
		float minA,
		float maxA,
		float minB,
		float maxB
	) noexcept
	{
		return minA < maxB && maxA > minB;
	}

	inline void TryMoveAlongXAxis(
		float& x,
		float y,
		float deltaX,
		float halfExtent,
		const WorldBounds& worldBounds,
		std::span<const WallRect> wallRectList
	) noexcept
	{
		if (deltaX == 0.0F)
		{
			return;
		}

		float targetX = x + deltaX;
		targetX = std::clamp(targetX, worldBounds.minX + halfExtent, worldBounds.maxX - halfExtent);

		const float playerMinY = y - halfExtent;
		const float playerMaxY = y + halfExtent;

		if (deltaX > 0.0F)
		{
			float maxAllowedX = targetX;

			for (const WallRect& wallRect : wallRectList)
			{
				if (!IsRangeOverlapping(playerMinY, playerMaxY, wallRect.minY, wallRect.maxY))
				{
					continue;
				}

				const float wallContactX = wallRect.minX - halfExtent;
				if (x <= wallContactX)
				{
					maxAllowedX = std::min<float>(maxAllowedX, wallContactX);
				}
			}

			x = maxAllowedX;
		}
		else
		{
			float minAllowedX = targetX;

			for (const WallRect& wallRect : wallRectList)
			{
				if (!IsRangeOverlapping(playerMinY, playerMaxY, wallRect.minY, wallRect.maxY))
				{
					continue;
				}

				const float wallContactX = wallRect.maxX + halfExtent;
				if (x >= wallContactX)
				{
					minAllowedX = std::max(minAllowedX, wallContactX);
				}
			}

			x = minAllowedX;
		}
	}

	inline void TryMoveAlongYAxis(
		float x,
		float& y,
		float deltaY,
		float halfExtent,
		const WorldBounds& worldBounds,
		std::span<const WallRect> wallRectList
	) noexcept
	{
		if (deltaY == 0.0F)
		{
			return;
		}

		float targetY = y + deltaY;
		targetY = std::clamp(targetY, worldBounds.minY + halfExtent, worldBounds.maxY - halfExtent);

		const float playerMinX = x - halfExtent;
		const float playerMaxX = x + halfExtent;

		if (deltaY > 0.0F)
		{
			float maxAllowedY = targetY;

			for (const WallRect& wallRect : wallRectList)
			{
				if (!IsRangeOverlapping(playerMinX, playerMaxX, wallRect.minX, wallRect.maxX))
				{
					continue;
				}

				const float wallContactY = wallRect.minY - halfExtent;
				if (y <= wallContactY)
				{
					maxAllowedY = std::min<float>(maxAllowedY, wallContactY);
				}
			}

			y = maxAllowedY;
		}
		else
		{
			float minAllowedY = targetY;

			for (const WallRect& wallRect : wallRectList)
			{
				if (!IsRangeOverlapping(playerMinX, playerMaxX, wallRect.minX, wallRect.maxX))
				{
					continue;
				}

				const float wallContactY = wallRect.maxY + halfExtent;
				if (y >= wallContactY)
				{
					minAllowedY = std::max(minAllowedY, wallContactY);
				}
			}

			y = minAllowedY;
		}
	}

	// X/Y 축을 순차적으로 이동시켜 벽과 충돌한 축만 제한하고
	// 다른 축의 이동은 유지해 벽을 따라 이동할 수 있는 함수
	inline void MovePlayerWithWallCollision(
		float& x,
		float& y,
		InputFlags inputFlags,
		float deltaSeconds,
		float moveSpeed,
		float halfExtent,
		const WorldBounds& worldBounds,
		std::span<const WallRect> wallRectList
	) noexcept
	{
		const MoveDirection direction = BuildNormalizedMoveDirection(inputFlags);

		const float moveDistance = moveSpeed * deltaSeconds;
		const float deltaX = direction.x * moveDistance;
		const float deltaY = direction.y * moveDistance;

		TryMoveAlongXAxis(
			x,
			y,
			deltaX,
			halfExtent,
			worldBounds,
			wallRectList
		);

		TryMoveAlongYAxis(
			x,
			y,
			deltaY,
			halfExtent,
			worldBounds,
			wallRectList
		);
	}

	[[nodiscard]] inline bool IsCircleOutsideWorldBounds(
		float x,
		float y,
		float radius,
		const WorldBounds& worldBounds
	) noexcept
	{
		return x < worldBounds.minX + radius
			|| x > worldBounds.maxX - radius
			|| y < worldBounds.minY + radius
			|| y > worldBounds.maxY - radius;
	}

	[[nodiscard]] inline bool IsCircleCollidingWithWall(
		float x,
		float y,
		float radius,
		const WallRect& wallRect
	) noexcept
	{
		const float nearestX = std::clamp(x, wallRect.minX, wallRect.maxX);
		const float nearestY = std::clamp(y, wallRect.minY, wallRect.maxY);

		const float deltaX = x - nearestX;
		const float deltaY = y - nearestY;

		return (deltaX * deltaX) + (deltaY * deltaY) <= (radius * radius);
	}

	[[nodiscard]] inline bool IsCircleCollidingWithAnyWall(
		float x,
		float y,
		float radius,
		std::span<const WallRect> wallRectList
	) noexcept
	{
		for (const WallRect& wallRect : wallRectList)
		{
			if (IsCircleCollidingWithWall(x, y, radius, wallRect))
			{
				return true;
			}
		}

		return false;
	}

	[[nodiscard]] inline bool IsCircleCollidingWithWorldOrWalls(
		float x,
		float y,
		float radius,
		const WorldBounds& worldBounds,
		std::span<const WallRect> wallRectList
	) noexcept
	{
		return IsCircleOutsideWorldBounds(x, y, radius, worldBounds)
			|| IsCircleCollidingWithAnyWall(x, y, radius, wallRectList);
	}

	// 이동 끝점이 충돌한 경우 이동 구간을 이분 탐색(Binary Search)해
	// 최초 충돌 경계에 가까운 위치를 근사해주는 함수
	inline void FindCircleImpactPosition(
		float startX,
		float startY,
		float endX,
		float endY,
		float radius,
		const WorldBounds& worldBounds,
		std::span<const WallRect> wallRectList,
		float& impactX,
		float& impactY
	) noexcept
	{
		impactX = endX;
		impactY = endY;

		if (!IsCircleCollidingWithWorldOrWalls(
			endX,
			endY,
			radius,
			worldBounds,
			wallRectList
		))
		{
			return;
		}

		float left = 0.0F;
		float right = 1.0F;

		for (int iteration = 0; iteration < 12; ++iteration)
		{
			const float mid = (left + right) * 0.5F;
			const float testX = startX + ((endX - startX) * mid);
			const float testY = startY + ((endY - startY) * mid);

			if (IsCircleCollidingWithWorldOrWalls(
				testX,
				testY,
				radius,
				worldBounds,
				wallRectList
			))
			{
				right = mid;
			}
			else
			{
				left = mid;
			}
		}

		impactX = startX + ((endX - startX) * right);
		impactY = startY + ((endY - startY) * right);
	}
}