#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "GameTypes.h"
#include "WorldCollision.h"

namespace common::game
{
	struct SpawnPoint
	{
	public:
		float x = 0.0F;
		float y = 0.0F;
	};

	inline constexpr std::array<WallRect, 3> room1WallRectList{
		WallRect{.minX = 120.0F, .minY = 120.0F, .maxX = 180.0F, .maxY = 560.0F },
		WallRect{.minX = 460.0F, .minY = 80.0F, .maxX = 520.0F, .maxY = 420.0F },
		WallRect{.minX = 800.0F, .minY = 220.0F, .maxX = 860.0F, .maxY = 620.0F }
	};

	inline constexpr std::array<SpawnPoint, 6> room1SpawnPointList{
		SpawnPoint{.x = 60.0F, .y = 60.0F },
		SpawnPoint{.x = 60.0F, .y = 640.0F },
		SpawnPoint{.x = 1140.0F, .y = 60.0F },
		SpawnPoint{.x = 1140.0F, .y = 640.0F },
		SpawnPoint{.x = 320.0F, .y = 350.0F },
		SpawnPoint{.x = 980.0F, .y = 350.0F }
	};

	inline constexpr std::array<WallRect, 4> room2WallRectList{
		WallRect{.minX = 180.0F, .minY = 120.0F, .maxX = 260.0F, .maxY = 200.0F },
		WallRect{.minX = 180.0F, .minY = 320.0F, .maxX = 260.0F, .maxY = 400.0F },
		WallRect{.minX = 560.0F, .minY = 140.0F, .maxX = 640.0F, .maxY = 520.0F },
		WallRect{.minX = 920.0F, .minY = 260.0F, .maxX = 1000.0F, .maxY = 340.0F }
	};

	inline constexpr std::array<SpawnPoint, 6> room2SpawnPointList{
		SpawnPoint{.x = 80.0F, .y = 80.0F },
		SpawnPoint{.x = 80.0F, .y = 620.0F },
		SpawnPoint{.x = 1110.0F, .y = 80.0F },
		SpawnPoint{.x = 1110.0F, .y = 620.0F },
		SpawnPoint{.x = 380.0F, .y = 100.0F },
		SpawnPoint{.x = 780.0F, .y = 600.0F }
	};

	inline constexpr std::array<WallRect, 5> room3WallRectList{
		WallRect{.minX = 160.0F, .minY = 160.0F, .maxX = 1040.0F, .maxY = 220.0F },
		WallRect{.minX = 160.0F, .minY = 480.0F, .maxX = 1040.0F, .maxY = 540.0F },
		WallRect{.minX = 280.0F, .minY = 260.0F, .maxX = 360.0F, .maxY = 440.0F },
		WallRect{.minX = 560.0F, .minY = 260.0F, .maxX = 640.0F, .maxY = 440.0F },
		WallRect{.minX = 840.0F, .minY = 260.0F, .maxX = 920.0F, .maxY = 440.0F }
	};

	inline constexpr std::array<SpawnPoint, 6> room3SpawnPointList{
		SpawnPoint{.x = 80.0F, .y = 350.0F },
		SpawnPoint{.x = 1120.0F, .y = 350.0F },
		SpawnPoint{.x = 220.0F, .y = 100.0F },
		SpawnPoint{.x = 220.0F, .y = 600.0F },
		SpawnPoint{.x = 980.0F, .y = 100.0F },
		SpawnPoint{.x = 980.0F, .y = 600.0F }
	};

	[[nodiscard]] inline std::span<const WallRect> GetWallRectListForRoom(RoomId roomId) noexcept
	{
		switch (roomId)
		{
		case 1:
			return room1WallRectList;

		case 2:
			return room2WallRectList;

		case 3:
			return room3WallRectList;

		default:
			return {};
		}
	}

	[[nodiscard]] inline std::span<const SpawnPoint> GetSpawnPointListForRoom(RoomId roomId) noexcept
	{
		switch (roomId)
		{
		case 1:
			return room1SpawnPointList;

		case 2:
			return room2SpawnPointList;

		case 3:
			return room3SpawnPointList;

		default:
			return {};
		}
	}
}