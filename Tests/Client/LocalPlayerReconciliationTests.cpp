#include "LocalPlayerReconciliationTests.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include <Common/Game/InputFlags.h>
#include <Common/Game/SimulationConstants.h>

#include <Client/Game/LocalPlayerReconciliation.h>

namespace
{
	inline constexpr float floatTolerance = 0.001F;

	[[nodiscard]] bool IsNearlyEqual(float lhs, float rhs) noexcept
	{
		return std::abs(lhs - rhs) <= floatTolerance;
	}

	void RunDefaultStateTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerReconciliation reconciliation;

		tests::Expect(
			result,
			!reconciliation.IsInitialized(),
			"LocalPlayerReconciliation: default state not initialized"
		);

		tests::Expect(
			result,
			reconciliation.GetPendingInputCount() == 0,
			"LocalPlayerReconciliation: default pending input empty"
		);
	}

	void RunPredictionTickTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerReconciliation reconciliation;

		reconciliation.Reset(
			320.0F,
			350.0F
		);

		reconciliation.ApplyPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		const float expectedX =
			320.0F +
			(
				common::game::defaultMoveSpeed *
				common::game::defaultFixedDeltaSeconds
				);

		tests::Expect(
			result,
			reconciliation.IsInitialized(),
			"LocalPlayerReconciliation: prediction initialized"
		);

		tests::Expect(
			result,
			reconciliation.GetPendingInputCount() == 1,
			"LocalPlayerReconciliation: prediction stores pending input"
		);

		tests::Expect(
			result,
			IsNearlyEqual(
				reconciliation.GetPredictedX(),
				expectedX
			),
			"LocalPlayerReconciliation: prediction updates x"
		);

		tests::Expect(
			result,
			IsNearlyEqual(
				reconciliation.GetPredictedY(),
				350.0F
			),
			"LocalPlayerReconciliation: prediction preserves y"
		);
	}

	void RunProcessedInputRemovalAndReplayTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerReconciliation reconciliation;

		reconciliation.Reset(
			320.0F,
			350.0F
		);

		reconciliation.ApplyPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		reconciliation.ApplyPredictionTick(
			2,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		const float moveDistance =
			common::game::defaultMoveSpeed *
			common::game::defaultFixedDeltaSeconds;

		reconciliation.Reconcile(
			320.0F + moveDistance,
			350.0F,
			1,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			reconciliation.GetPendingInputCount() == 1,
			"LocalPlayerReconciliation: processed input removed"
		);

		tests::Expect(
			result,
			IsNearlyEqual(
				reconciliation.GetPredictedX(),
				320.0F + (moveDistance * 2.0F)
			),
			"LocalPlayerReconciliation: remaining input replayed"
		);
	}

	void RunWrappedSequenceReplayTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerReconciliation reconciliation;

		reconciliation.Reset(
			320.0F,
			350.0F
		);

		constexpr std::uint32_t maxSequence =
			std::numeric_limits<std::uint32_t>::max();

		reconciliation.ApplyPredictionTick(
			maxSequence,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		reconciliation.ApplyPredictionTick(
			0,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		reconciliation.ApplyPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		const float moveDistance =
			common::game::defaultMoveSpeed *
			common::game::defaultFixedDeltaSeconds;

		reconciliation.Reconcile(
			320.0F + moveDistance,
			350.0F,
			maxSequence,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			reconciliation.GetPendingInputCount() == 2,
			"LocalPlayerReconciliation: wrapped pending inputs preserved"
		);

		tests::Expect(
			result,
			IsNearlyEqual(
				reconciliation.GetPredictedX(),
				320.0F + (moveDistance * 3.0F)
			),
			"LocalPlayerReconciliation: wrapped inputs replayed"
		);
	}

	void RunModerateCorrectionPreservesRenderPositionTest(
		tests::DebugTestResult& result
	)
	{
		client::game::LocalPlayerReconciliation reconciliation;

		reconciliation.Reset(
			320.0F,
			350.0F
		);

		reconciliation.ApplyPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		const float oldRenderX =
			reconciliation.GetRenderX();

		reconciliation.Reconcile(
			315.0F,
			350.0F,
			1,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(
				reconciliation.GetPredictedX(),
				315.0F
			),
			"LocalPlayerReconciliation: moderate correction updates prediction"
		);

		tests::Expect(
			result,
			IsNearlyEqual(
				reconciliation.GetRenderX(),
				oldRenderX
			),
			"LocalPlayerReconciliation: moderate correction preserves visual position"
		);

		reconciliation.UpdateRenderCorrection(
			common::game::defaultFixedDeltaSeconds
		);

		tests::Expect(
			result,
			reconciliation.GetRenderX() < oldRenderX,
			"LocalPlayerReconciliation: render correction moves toward authoritative position"
		);

		tests::Expect(
			result,
			reconciliation.GetRenderX() > 315.0F,
			"LocalPlayerReconciliation: render correction remains smooth"
		);
	}

	void RunHardSnapTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerReconciliation reconciliation;

		reconciliation.Reset(
			320.0F,
			350.0F
		);

		reconciliation.Reconcile(
			600.0F,
			350.0F,
			0,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(
				reconciliation.GetPredictedX(),
				600.0F
			),
			"LocalPlayerReconciliation: hard snap updates prediction"
		);

		tests::Expect(
			result,
			IsNearlyEqual(
				reconciliation.GetRenderX(),
				600.0F
			),
			"LocalPlayerReconciliation: hard snap clears render correction"
		);
	}

	void RunResetClearsPendingInputTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerReconciliation reconciliation;

		reconciliation.Reset(
			320.0F,
			350.0F
		);

		reconciliation.ApplyPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		reconciliation.Reset(
			100.0F,
			200.0F
		);

		tests::Expect(
			result,
			reconciliation.GetPendingInputCount() == 0,
			"LocalPlayerReconciliation: reset clears pending input"
		);

		tests::Expect(
			result,
			IsNearlyEqual(
				reconciliation.GetRenderX(),
				100.0F
			),
			"LocalPlayerReconciliation: reset updates render x"
		);

		tests::Expect(
			result,
			IsNearlyEqual(
				reconciliation.GetRenderY(),
				200.0F
			),
			"LocalPlayerReconciliation: reset updates render y"
		);
	}

	void RunClearTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerReconciliation reconciliation;

		reconciliation.Reset(
			320.0F,
			350.0F
		);

		reconciliation.ApplyPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		reconciliation.Clear();

		tests::Expect(
			result,
			!reconciliation.IsInitialized(),
			"LocalPlayerReconciliation: clear resets initialization"
		);

		tests::Expect(
			result,
			reconciliation.GetPendingInputCount() == 0,
			"LocalPlayerReconciliation: clear removes pending input"
		);
	}
}

namespace tests::client
{
	DebugTestResult RunLocalPlayerReconciliationTests()
	{
		DebugTestResult result{};

		RunDefaultStateTest(result);
		RunPredictionTickTest(result);
		RunProcessedInputRemovalAndReplayTest(result);
		RunWrappedSequenceReplayTest(result);
		RunModerateCorrectionPreservesRenderPositionTest(result);
		RunHardSnapTest(result);
		RunResetClearsPendingInputTest(result);
		RunClearTest(result);

		return result;
	}
}