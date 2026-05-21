#include "WorldCollisionTests.h"

#include <array>
#include <cmath>
#include <span>

#include <Common/Game/InputFlags.h>
#include <Common/Game/SimulationConstants.h>
#include <Common/Game/WorldCollision.h>

#include <Tests/DebugTestResult.h>

namespace
{
	[[nodiscard]] bool IsNearlyEqual(float lhs, float rhs, float epsilon = 0.01F) noexcept
	{
		return std::fabs(lhs - rhs) <= epsilon;
	}

	[[nodiscard]] std::span<const common::game::WallRect> MakeWallSpan(const std::array<common::game::WallRect, 1>& wallRectList) noexcept
	{
		return std::span<const common::game::WallRect>(wallRectList.data(), wallRectList.size());
	}

	void RunClampPositionToWorldBoundsTest(tests::DebugTestResult& result)
	{
		common::game::WorldBounds worldBounds{};
		worldBounds.minX = 0.0F;
		worldBounds.minY = 0.0F;
		worldBounds.maxX = 100.0F;
		worldBounds.maxY = 80.0F;

		float x = -50.0F;
		float y = 200.0F;

		common::game::ClampPositionToWorldBounds(x, y, 10.0F, worldBounds);

		tests::Expect(result, IsNearlyEqual(x, 10.0F), "WorldCollision: clamp x to min");
		tests::Expect(result, IsNearlyEqual(y, 70.0F), "WorldCollision: clamp y to max");
	}

	void RunAabbWallOverlapTest(tests::DebugTestResult& result)
	{
		const common::game::WallRect wallRect{
			.minX = 40.0F,
			.minY = 40.0F,
			.maxX = 80.0F,
			.maxY = 80.0F
		};

		const bool isOverlapping = common::game::IsAabbOverlappingWall(30.0F, 30.0F, 50.0F, 50.0F, wallRect);
		const bool isTouchingOnly = common::game::IsAabbOverlappingWall(10.0F, 10.0F, 40.0F, 40.0F, wallRect);
		const bool isSeparated = common::game::IsAabbOverlappingWall(0.0F, 0.0F, 20.0F, 20.0F, wallRect);

		tests::Expect(result, isOverlapping, "WorldCollision: aabb overlaps wall");
		tests::Expect(result, !isTouchingOnly, "WorldCollision: aabb touching edge is not overlap");
		tests::Expect(result, !isSeparated, "WorldCollision: aabb separated from wall");
	}

	void RunPlayerWallCollisionTest(tests::DebugTestResult& result)
	{
		const common::game::WallRect wallRect{
			.minX = 40.0F,
			.minY = 40.0F,
			.maxX = 80.0F,
			.maxY = 80.0F
		};

		const bool isColliding = common::game::IsPlayerCollidingWithWall(35.0F, 60.0F, 10.0F, wallRect);
		const bool isNotColliding = common::game::IsPlayerCollidingWithWall(20.0F, 60.0F, 10.0F, wallRect);

		tests::Expect(result, isColliding, "WorldCollision: player collides with wall");
		tests::Expect(result, !isNotColliding, "WorldCollision: player does not collide with wall");
	}

	void RunTryMoveAlongXAxisBlockedByWallTest(tests::DebugTestResult& result)
	{
		const common::game::WorldBounds worldBounds{
			.minX = 0.0F,
			.minY = 0.0F,
			.maxX = 200.0F,
			.maxY = 200.0F
		};

		const std::array<common::game::WallRect, 1> wallRectList{
			common::game::WallRect{.minX = 100.0F, .minY = 40.0F, .maxX = 130.0F, .maxY = 100.0F }
		};

		float x = 70.0F;
		const float y = 60.0F;

		common::game::TryMoveAlongXAxis(x, y, 50.0F, 10.0F, worldBounds, MakeWallSpan(wallRectList));

		tests::Expect(result, IsNearlyEqual(x, 90.0F), "WorldCollision: x movement blocked by wall");
	}

	void RunTryMoveAlongYAxisBlockedByWallTest(tests::DebugTestResult& result)
	{
		const common::game::WorldBounds worldBounds{
			.minX = 0.0F,
			.minY = 0.0F,
			.maxX = 200.0F,
			.maxY = 200.0F
		};

		const std::array<common::game::WallRect, 1> wallRectList{
			common::game::WallRect{.minX = 40.0F, .minY = 100.0F, .maxX = 100.0F, .maxY = 130.0F }
		};

		const float x = 60.0F;
		float y = 70.0F;

		common::game::TryMoveAlongYAxis(x, y, 50.0F, 10.0F, worldBounds, MakeWallSpan(wallRectList));

		tests::Expect(result, IsNearlyEqual(y, 90.0F), "WorldCollision: y movement blocked by wall");
	}

	void RunMovePlayerWithWallCollisionTest(tests::DebugTestResult& result)
	{
		const common::game::WorldBounds worldBounds{
			.minX = 0.0F,
			.minY = 0.0F,
			.maxX = 200.0F,
			.maxY = 200.0F
		};

		const std::array<common::game::WallRect, 1> wallRectList{
			common::game::WallRect{.minX = 100.0F, .minY = 40.0F, .maxX = 130.0F, .maxY = 100.0F }
		};

		float x = 70.0F;
		float y = 60.0F;

		common::game::MovePlayerWithWallCollision(
			x,
			y,
			common::game::InputFlags::Right,
			1.0F,
			100.0F,
			10.0F,
			worldBounds,
			MakeWallSpan(wallRectList)
		);

		tests::Expect(result, IsNearlyEqual(x, 90.0F), "WorldCollision: player movement blocked by wall");
		tests::Expect(result, IsNearlyEqual(y, 60.0F), "WorldCollision: player y unchanged");
	}

	void RunCircleOutsideWorldBoundsTest(tests::DebugTestResult& result)
	{
		const common::game::WorldBounds worldBounds{
			.minX = 0.0F,
			.minY = 0.0F,
			.maxX = 100.0F,
			.maxY = 100.0F
		};

		const bool inside = common::game::IsCircleOutsideWorldBounds(50.0F, 50.0F, 5.0F, worldBounds);
		const bool outsideLeft = common::game::IsCircleOutsideWorldBounds(4.0F, 50.0F, 5.0F, worldBounds);
		const bool outsideRight = common::game::IsCircleOutsideWorldBounds(96.0F, 50.0F, 5.0F, worldBounds);
		const bool outsideTop = common::game::IsCircleOutsideWorldBounds(50.0F, 4.0F, 5.0F, worldBounds);
		const bool outsideBottom = common::game::IsCircleOutsideWorldBounds(50.0F, 96.0F, 5.0F, worldBounds);

		tests::Expect(result, !inside, "WorldCollision: circle inside world");
		tests::Expect(result, outsideLeft, "WorldCollision: circle outside left");
		tests::Expect(result, outsideRight, "WorldCollision: circle outside right");
		tests::Expect(result, outsideTop, "WorldCollision: circle outside top");
		tests::Expect(result, outsideBottom, "WorldCollision: circle outside bottom");
	}

	void RunCircleWallCollisionTest(tests::DebugTestResult& result)
	{
		const common::game::WallRect wallRect{
			.minX = 40.0F,
			.minY = 40.0F,
			.maxX = 80.0F,
			.maxY = 80.0F
		};

		const bool centerInside = common::game::IsCircleCollidingWithWall(60.0F, 60.0F, 5.0F, wallRect);
		const bool touchingSide = common::game::IsCircleCollidingWithWall(35.0F, 60.0F, 5.0F, wallRect);
		const bool separated = common::game::IsCircleCollidingWithWall(30.0F, 60.0F, 5.0F, wallRect);

		tests::Expect(result, centerInside, "WorldCollision: circle center inside wall");
		tests::Expect(result, touchingSide, "WorldCollision: circle touching wall side");
		tests::Expect(result, !separated, "WorldCollision: circle separated from wall");
	}

	void RunCircleWorldOrWallsCollisionTest(tests::DebugTestResult& result)
	{
		const common::game::WorldBounds worldBounds{
			.minX = 0.0F,
			.minY = 0.0F,
			.maxX = 200.0F,
			.maxY = 200.0F
		};

		const std::array<common::game::WallRect, 1> wallRectList{
			common::game::WallRect{.minX = 80.0F, .minY = 80.0F, .maxX = 120.0F, .maxY = 120.0F }
		};

		const bool clear = common::game::IsCircleCollidingWithWorldOrWalls(30.0F, 30.0F, 5.0F, worldBounds, MakeWallSpan(wallRectList));
		const bool wall = common::game::IsCircleCollidingWithWorldOrWalls(90.0F, 90.0F, 5.0F, worldBounds, MakeWallSpan(wallRectList));
		const bool world = common::game::IsCircleCollidingWithWorldOrWalls(2.0F, 30.0F, 5.0F, worldBounds, MakeWallSpan(wallRectList));

		tests::Expect(result, !clear, "WorldCollision: circle clear from world and walls");
		tests::Expect(result, wall, "WorldCollision: circle collides with wall through combined check");
		tests::Expect(result, world, "WorldCollision: circle collides with world through combined check");
	}

	void RunFindCircleImpactPositionWorldBoundsTest(tests::DebugTestResult& result)
	{
		const common::game::WorldBounds worldBounds{
			.minX = 0.0F,
			.minY = 0.0F,
			.maxX = 100.0F,
			.maxY = 100.0F
		};

		const std::array<common::game::WallRect, 0> wallRectList{};

		float impactX = 0.0F;
		float impactY = 0.0F;

		common::game::FindCircleImpactPosition(
			50.0F,
			50.0F,
			120.0F,
			50.0F,
			5.0F,
			worldBounds,
			std::span<const common::game::WallRect>(wallRectList.data(), wallRectList.size()),
			impactX,
			impactY
		);

		tests::Expect(result, IsNearlyEqual(impactX, 95.0F, 0.1F), "WorldCollision: impact x on world right boundary");
		tests::Expect(result, IsNearlyEqual(impactY, 50.0F, 0.1F), "WorldCollision: impact y on world right boundary");
	}

	void RunFindCircleImpactPositionWallTest(tests::DebugTestResult& result)
	{
		const common::game::WorldBounds worldBounds{
			.minX = 0.0F,
			.minY = 0.0F,
			.maxX = 200.0F,
			.maxY = 200.0F
		};

		const std::array<common::game::WallRect, 1> wallRectList{
			common::game::WallRect{.minX = 100.0F, .minY = 40.0F, .maxX = 130.0F, .maxY = 100.0F }
		};

		float impactX = 0.0F;
		float impactY = 0.0F;

		common::game::FindCircleImpactPosition(
			50.0F,
			60.0F,
			120.0F,
			60.0F,
			5.0F,
			worldBounds,
			MakeWallSpan(wallRectList),
			impactX,
			impactY
		);

		tests::Expect(result, IsNearlyEqual(impactX, 95.0F, 0.1F), "WorldCollision: impact x on wall left side");
		tests::Expect(result, IsNearlyEqual(impactY, 60.0F, 0.1F), "WorldCollision: impact y on wall left side");
	}
}

namespace tests::game
{
	tests::DebugTestResult RunWorldCollisionTests()
	{
		tests::DebugTestResult result{};

		RunClampPositionToWorldBoundsTest(result);
		RunAabbWallOverlapTest(result);
		RunPlayerWallCollisionTest(result);
		RunTryMoveAlongXAxisBlockedByWallTest(result);
		RunTryMoveAlongYAxisBlockedByWallTest(result);
		RunMovePlayerWithWallCollisionTest(result);
		RunCircleOutsideWorldBoundsTest(result);
		RunCircleWallCollisionTest(result);
		RunCircleWorldOrWallsCollisionTest(result);
		RunFindCircleImpactPositionWorldBoundsTest(result);
		RunFindCircleImpactPositionWallTest(result);

		return result;
	}
}