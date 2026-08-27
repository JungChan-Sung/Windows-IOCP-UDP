#include "LocalPlayerPredictionTests.h"

#include <cmath>

#include <Common/Game/InputFlags.h>
#include <Common/Game/SimulationConstants.h>

#include <Client/Game/LocalPlayerPrediction.h>

#include <Tests/DebugTestResult.h>

namespace
{
	inline constexpr float floatTolerance = 0.001F;

	[[nodiscard]] bool IsNearlyEqual(float left, float right) noexcept
	{
		return std::abs(left - right) <= floatTolerance;
	}

	void RunDefaultStateTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;

		tests::Expect(result, !prediction.IsInitialized(), "LocalPlayerPrediction: default state is not initialized");
		tests::Expect(result, IsNearlyEqual(prediction.GetX(), 0.0F), "LocalPlayerPrediction: default x");
		tests::Expect(result, IsNearlyEqual(prediction.GetY(), 0.0F), "LocalPlayerPrediction: default y");
	}

	void RunResetTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;

		prediction.Reset(320.0F, 350.0F);

		tests::Expect(result, prediction.IsInitialized(), "LocalPlayerPrediction: reset initializes prediction");
		tests::Expect(result, IsNearlyEqual(prediction.GetX(), 320.0F), "LocalPlayerPrediction: reset x");
		tests::Expect(result, IsNearlyEqual(prediction.GetY(), 350.0F), "LocalPlayerPrediction: reset y");
	}

	void RunUninitializedInputIgnoredTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;

		prediction.ApplyInput(
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(result, !prediction.IsInitialized(), "LocalPlayerPrediction: uninitialized input remains uninitialized");
		tests::Expect(result, IsNearlyEqual(prediction.GetX(), 0.0F), "LocalPlayerPrediction: uninitialized input does not move x");
		tests::Expect(result, IsNearlyEqual(prediction.GetY(), 0.0F), "LocalPlayerPrediction: uninitialized input does not move y");
	}

	void RunHorizontalMovementTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;

		prediction.Reset(320.0F, 350.0F);

		prediction.ApplyInput(
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		const float expectedX =
			320.0F + (common::game::defaultMoveSpeed * common::game::defaultFixedDeltaSeconds);

		tests::Expect(result, IsNearlyEqual(prediction.GetX(), expectedX), "LocalPlayerPrediction: horizontal movement x");
		tests::Expect(result, IsNearlyEqual(prediction.GetY(), 350.0F), "LocalPlayerPrediction: horizontal movement y");
	}

	void RunDiagonalMovementNormalizedTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;

		prediction.Reset(320.0F, 350.0F);

		const common::game::InputFlags inputFlags =
			common::game::InputFlags::Up | common::game::InputFlags::Right;

		prediction.ApplyInput(
			inputFlags,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		const float moveDistance =
			common::game::defaultMoveSpeed * common::game::defaultFixedDeltaSeconds;

		const float axisDistance = moveDistance / std::sqrt(2.0F);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), 320.0F + axisDistance),
			"LocalPlayerPrediction: diagonal movement x normalized"
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetY(), 350.0F - axisDistance),
			"LocalPlayerPrediction: diagonal movement y normalized"
		);
	}

	void RunWallCollisionTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;

		prediction.Reset(100.0F, 200.0F);

		prediction.ApplyInput(
			common::game::InputFlags::Right,
			0.1F,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), 110.0F),
			"LocalPlayerPrediction: movement stops at wall"
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetY(), 200.0F),
			"LocalPlayerPrediction: wall collision preserves y"
		);
	}

	void RunWorldBoundsTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;

		prediction.Reset(20.0F, 60.0F);

		prediction.ApplyInput(
			common::game::InputFlags::Left,
			1.0F,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), common::game::playerHalfExtent),
			"LocalPlayerPrediction: movement clamped to world bounds"
		);
	}

	void RunNonPositiveDeltaIgnoredTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;

		prediction.Reset(320.0F, 350.0F);

		prediction.ApplyInput(
			common::game::InputFlags::Right,
			0.0F,
			common::game::defaultMoveSpeed,
			1
		);

		prediction.ApplyInput(
			common::game::InputFlags::Right,
			-0.05F,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(result, IsNearlyEqual(prediction.GetX(), 320.0F), "LocalPlayerPrediction: non-positive delta does not move x");
		tests::Expect(result, IsNearlyEqual(prediction.GetY(), 350.0F), "LocalPlayerPrediction: non-positive delta does not move y");
	}

	void RunClearTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;

		prediction.Reset(320.0F, 350.0F);
		prediction.Clear();

		prediction.ApplyInput(
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(result, !prediction.IsInitialized(), "LocalPlayerPrediction: clear resets initialization");
		tests::Expect(result, IsNearlyEqual(prediction.GetX(), 0.0F), "LocalPlayerPrediction: clear x");
		tests::Expect(result, IsNearlyEqual(prediction.GetY(), 0.0F), "LocalPlayerPrediction: clear y");
	}
}

namespace tests::client
{
	DebugTestResult RunLocalPlayerPredictionTests()
	{
		DebugTestResult result{};

		RunDefaultStateTest(result);
		RunResetTest(result);
		RunUninitializedInputIgnoredTest(result);
		RunHorizontalMovementTest(result);
		RunDiagonalMovementNormalizedTest(result);
		RunWallCollisionTest(result);
		RunWorldBoundsTest(result);
		RunNonPositiveDeltaIgnoredTest(result);
		RunClearTest(result);

		return result;
	}
}