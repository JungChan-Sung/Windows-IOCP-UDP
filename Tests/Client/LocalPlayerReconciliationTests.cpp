#include "LocalPlayerReconciliationTests.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include <Common/Game/InputFlags.h>
#include <Common/Game/SimulationConstants.h>

#include <Client/Game/LocalPlayerPrediction.h>
#include <Client/Game/LocalPlayerReconciliation.h>

namespace
{
	inline constexpr float floatTolerance = 0.001F;

	[[nodiscard]] bool IsNearlyEqual(float lhs, float rhs) noexcept
	{
		return std::abs(lhs - rhs) <= floatTolerance;
	}

	[[nodiscard]] float GetRenderX(
		const client::game::LocalPlayerPrediction& prediction,
		const client::game::LocalPlayerReconciliation& reconciliation
	) noexcept
	{
		return prediction.GetX() + reconciliation.GetRenderCorrectionOffsetX();
	}

	[[nodiscard]] float GetRenderY(
		const client::game::LocalPlayerPrediction& prediction,
		const client::game::LocalPlayerReconciliation& reconciliation
	) noexcept
	{
		return prediction.GetY() + reconciliation.GetRenderCorrectionOffsetY();
	}

	void RunDefaultStateTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerReconciliation reconciliation;

		tests::Expect(
			result,
			IsNearlyEqual(reconciliation.GetRenderCorrectionOffsetX(), 0.0F),
			"LocalPlayerReconciliation: default correction x is zero"
		);

		tests::Expect(
			result,
			IsNearlyEqual(reconciliation.GetRenderCorrectionOffsetY(), 0.0F),
			"LocalPlayerReconciliation: default correction y is zero"
		);
	}

	void RunProcessedInputRemovalAndReplayTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;
		client::game::LocalPlayerReconciliation reconciliation;

		prediction.Reset(320.0F, 350.0F);

		reconciliation.RecordPendingInput(1, common::game::InputFlags::Right, common::game::defaultFixedDeltaSeconds);
		prediction.ApplyInput(
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		reconciliation.RecordPendingInput(2, common::game::InputFlags::Right, common::game::defaultFixedDeltaSeconds);
		prediction.ApplyInput(
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		const float moveDistance = common::game::defaultMoveSpeed * common::game::defaultFixedDeltaSeconds;

		reconciliation.Reconcile(
			prediction,
			320.0F + moveDistance,
			350.0F,
			1,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), 320.0F + (moveDistance * 2.0F)),
			"LocalPlayerReconciliation: unprocessed input replayed"
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetY(), 350.0F),
			"LocalPlayerReconciliation: replay preserves y"
		);

		tests::Expect(
			result,
			IsNearlyEqual(reconciliation.GetRenderCorrectionOffsetX(), 0.0F),
			"LocalPlayerReconciliation: matching replay needs no correction"
		);
	}

	void RunWrappedSequenceReplayTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;
		client::game::LocalPlayerReconciliation reconciliation;

		prediction.Reset(320.0F, 350.0F);

		constexpr std::uint32_t maxSequence = std::numeric_limits<std::uint32_t>::max();

		reconciliation.RecordPendingInput(maxSequence, common::game::InputFlags::Right, common::game::defaultFixedDeltaSeconds);
		prediction.ApplyInput(
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		reconciliation.RecordPendingInput(0, common::game::InputFlags::Right, common::game::defaultFixedDeltaSeconds);
		prediction.ApplyInput(
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		reconciliation.RecordPendingInput(1, common::game::InputFlags::Right, common::game::defaultFixedDeltaSeconds);
		prediction.ApplyInput(
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		const float moveDistance = common::game::defaultMoveSpeed * common::game::defaultFixedDeltaSeconds;

		reconciliation.Reconcile(
			prediction,
			320.0F + moveDistance,
			350.0F,
			maxSequence,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), 320.0F + (moveDistance * 3.0F)),
			"LocalPlayerReconciliation: wrapped pending inputs replayed"
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetY(), 350.0F),
			"LocalPlayerReconciliation: wrapped replay preserves y"
		);
	}

	void RunSmallCorrectionIgnoredTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;
		client::game::LocalPlayerReconciliation reconciliation;

		prediction.Reset(320.0F, 350.0F);

		reconciliation.Reconcile(
			prediction,
			321.0F,
			350.0F,
			0,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), 320.0F),
			"LocalPlayerReconciliation: small correction ignored"
		);

		tests::Expect(
			result,
			IsNearlyEqual(reconciliation.GetRenderCorrectionOffsetX(), 0.0F),
			"LocalPlayerReconciliation: ignored correction creates no render offset"
		);
	}

	void RunModerateCorrectionTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;
		client::game::LocalPlayerReconciliation reconciliation;

		prediction.Reset(320.0F, 350.0F);

		reconciliation.RecordPendingInput(1, common::game::InputFlags::Right, common::game::defaultFixedDeltaSeconds);
		prediction.ApplyInput(
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		const float previousRenderX = prediction.GetX();

		reconciliation.Reconcile(
			prediction,
			315.0F,
			350.0F,
			1,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), 315.0F),
			"LocalPlayerReconciliation: moderate correction updates prediction"
		);

		tests::Expect(
			result,
			IsNearlyEqual(GetRenderX(prediction, reconciliation), previousRenderX),
			"LocalPlayerReconciliation: moderate correction preserves current render position"
		);

		reconciliation.UpdateRenderCorrection(common::game::defaultFixedDeltaSeconds);

		tests::Expect(
			result,
			GetRenderX(prediction, reconciliation) < previousRenderX,
			"LocalPlayerReconciliation: render correction moves toward reconciled position"
		);

		tests::Expect(
			result,
			GetRenderX(prediction, reconciliation) > prediction.GetX(),
			"LocalPlayerReconciliation: render correction remains smooth"
		);
	}

	void RunHardSnapTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;
		client::game::LocalPlayerReconciliation reconciliation;

		prediction.Reset(320.0F, 350.0F);

		reconciliation.Reconcile(
			prediction,
			600.0F,
			350.0F,
			0,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), 600.0F),
			"LocalPlayerReconciliation: hard snap updates prediction"
		);

		tests::Expect(
			result,
			IsNearlyEqual(GetRenderX(prediction, reconciliation), 600.0F),
			"LocalPlayerReconciliation: hard snap clears render correction"
		);

		tests::Expect(
			result,
			IsNearlyEqual(GetRenderY(prediction, reconciliation), 350.0F),
			"LocalPlayerReconciliation: hard snap preserves y"
		);
	}

	void RunClearRemovesPendingInputTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;
		client::game::LocalPlayerReconciliation reconciliation;

		prediction.Reset(320.0F, 350.0F);

		reconciliation.RecordPendingInput(1, common::game::InputFlags::Right, common::game::defaultFixedDeltaSeconds);

		reconciliation.Clear();

		reconciliation.Reconcile(
			prediction,
			100.0F,
			350.0F,
			0,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), 100.0F),
			"LocalPlayerReconciliation: clear removes pending replay input"
		);

		tests::Expect(
			result,
			IsNearlyEqual(reconciliation.GetRenderCorrectionOffsetX(), 0.0F),
			"LocalPlayerReconciliation: clear resets correction x"
		);

		tests::Expect(
			result,
			IsNearlyEqual(reconciliation.GetRenderCorrectionOffsetY(), 0.0F),
			"LocalPlayerReconciliation: clear resets correction y"
		);
	}

	void RunHistoryOverflowFallbackTest(tests::DebugTestResult& result)
	{
		client::game::LocalPlayerPrediction prediction;
		client::game::LocalPlayerReconciliation reconciliation;

		prediction.Reset(350.0F, 350.0F);

		for (std::uint32_t sequence = 1; sequence <= 257; ++sequence)
		{
			reconciliation.RecordPendingInput(sequence, common::game::InputFlags::None, common::game::defaultFixedDeltaSeconds);
		}

		reconciliation.Reconcile(
			prediction,
			320.0F,
			350.0F,
			0,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), 320.0F),
			"LocalPlayerReconciliation: history overflow resets prediction to authoritative x"
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetY(), 350.0F),
			"LocalPlayerReconciliation: history overflow resets prediction to authoritative y"
		);

		tests::Expect(
			result,
			IsNearlyEqual(reconciliation.GetRenderCorrectionOffsetX(), 0.0F),
			"LocalPlayerReconciliation: history overflow clears render correction x"
		);

		tests::Expect(
			result,
			IsNearlyEqual(reconciliation.GetRenderCorrectionOffsetY(), 0.0F),
			"LocalPlayerReconciliation: history overflow clears render correction y"
		);

		const float moveDistance = common::game::defaultMoveSpeed * common::game::defaultFixedDeltaSeconds;

		reconciliation.RecordPendingInput(258, common::game::InputFlags::Right, common::game::defaultFixedDeltaSeconds);
		prediction.ApplyInput(
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds,
			common::game::defaultMoveSpeed,
			1
		);

		reconciliation.Reconcile(
			prediction,
			320.0F,
			350.0F,
			257,
			common::game::defaultMoveSpeed,
			1
		);

		tests::Expect(
			result,
			IsNearlyEqual(prediction.GetX(), 320.0F + moveDistance),
			"LocalPlayerReconciliation: history tracking recovers after overflow fallback"
		);
	}
}

namespace tests::client
{
	DebugTestResult RunLocalPlayerReconciliationTests()
	{
		DebugTestResult result{};

		RunDefaultStateTest(result);
		RunProcessedInputRemovalAndReplayTest(result);
		RunWrappedSequenceReplayTest(result);
		RunSmallCorrectionIgnoredTest(result);
		RunModerateCorrectionTest(result);
		RunHardSnapTest(result);
		RunClearRemovesPendingInputTest(result);
		RunHistoryOverflowFallbackTest(result);

		return result;
	}
}