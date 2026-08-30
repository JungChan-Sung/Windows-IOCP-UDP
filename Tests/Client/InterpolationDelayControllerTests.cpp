#include "InterpolationDelayControllerTests.h"

#include <Common/Game/SimulationConstants.h>
#include <Common/Time/TimeTypes.h>

#include <Client/Game/InterpolationDelayController.h>

namespace
{
	void RunConfigureTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(100),
			common::time::Milliseconds(50),
			common::time::Milliseconds(250)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(100),
			"InterpolationDelayController: configure sets default delay"
		);

		tests::Expect(
			result,
			controller.GetMaxDelay() == common::time::Milliseconds(250),
			"InterpolationDelayController: configure sets max delay"
		);
	}

	void RunSetDelayClampTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(100),
			common::time::Milliseconds(50),
			common::time::Milliseconds(250)
		);

		controller.SetDelay(common::time::Milliseconds(20));

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(50),
			"InterpolationDelayController: delay clamps to minimum"
		);

		controller.SetDelay(common::time::Milliseconds(300));

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(250),
			"InterpolationDelayController: delay clamps to maximum"
		);
	}

	void RunStableTimingKeepsDelayTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(100),
			common::time::Milliseconds(0),
			common::time::Milliseconds(500)
		);

		common::time::TimePoint startTime;

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1000),
			startTime + common::time::Milliseconds(950),
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(100),
			"InterpolationDelayController: sufficient delay remains unchanged"
		);
	}

	void RunUnderflowRiskIncreasesDelayTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(100),
			common::time::Milliseconds(0),
			common::time::Milliseconds(500)
		);

		common::time::TimePoint startTime;

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1000),
			startTime + common::time::Milliseconds(850),
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(166),
			"InterpolationDelayController: underflow risk increases delay"
		);
	}

	void RunAutomaticIncreaseClampsToMaximumTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(100),
			common::time::Milliseconds(0),
			common::time::Milliseconds(250)
		);

		common::time::TimePoint startTime;

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1000),
			startTime,
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(250),
			"InterpolationDelayController: automatic increase clamps to maximum"
		);
	}

	void RunAutomaticDecreaseWaitsForStablePeriodTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(100),
			common::time::Milliseconds(0),
			common::time::Milliseconds(500)
		);

		common::time::TimePoint startTime;

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1000),
			startTime + common::time::Milliseconds(850),
			common::time::Milliseconds(16)
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1100),
			startTime + common::time::Milliseconds(1090),
			common::time::Milliseconds(16)
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(2099),
			startTime + common::time::Milliseconds(2089),
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(166),
			"InterpolationDelayController: automatic decrease waits for stable period"
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(2100),
			startTime + common::time::Milliseconds(2090),
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(156),
			"InterpolationDelayController: automatic decrease starts after stable period"
		);
	}

	void RunAutomaticDecreaseUsesIntervalTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(100),
			common::time::Milliseconds(0),
			common::time::Milliseconds(500)
		);

		common::time::TimePoint startTime;

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1000),
			startTime + common::time::Milliseconds(850),
			common::time::Milliseconds(16)
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1100),
			startTime + common::time::Milliseconds(1090),
			common::time::Milliseconds(16)
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(2100),
			startTime + common::time::Milliseconds(2090),
			common::time::Milliseconds(16)
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(2349),
			startTime + common::time::Milliseconds(2339),
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(156),
			"InterpolationDelayController: automatic decrease waits for decrease interval"
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(2350),
			startTime + common::time::Milliseconds(2340),
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(146),
			"InterpolationDelayController: automatic decrease repeats after interval"
		);
	}

	void RunAutomaticDecreaseStopsAtRequestedDelayTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(150),
			common::time::Milliseconds(0),
			common::time::Milliseconds(500)
		);

		common::time::TimePoint startTime;

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1000),
			startTime + common::time::Milliseconds(860),
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(156),
			"InterpolationDelayController: requested delay can be temporarily exceeded"
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1100),
			startTime + common::time::Milliseconds(1090),
			common::time::Milliseconds(16)
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(2100),
			startTime + common::time::Milliseconds(2090),
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(150),
			"InterpolationDelayController: automatic decrease stops at requested delay"
		);
	}

	void RunNewSpikeCancelsAutomaticDecreaseTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(100),
			common::time::Milliseconds(0),
			common::time::Milliseconds(500)
		);

		common::time::TimePoint startTime;

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1000),
			startTime + common::time::Milliseconds(850),
			common::time::Milliseconds(16)
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1100),
			startTime + common::time::Milliseconds(1090),
			common::time::Milliseconds(16)
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1500),
			startTime + common::time::Milliseconds(1300),
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(216),
			"InterpolationDelayController: new spike increases delay again"
		);

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(2100),
			startTime + common::time::Milliseconds(2090),
			common::time::Milliseconds(16)
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(216),
			"InterpolationDelayController: new spike restarts stable hold"
		);
	}

	void RunManualSetDelayOverridesAutomaticDelayTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(100),
			common::time::Milliseconds(0),
			common::time::Milliseconds(500)
		);

		common::time::TimePoint startTime;

		controller.ObserveSnapshotTiming(
			startTime + common::time::Milliseconds(1000),
			startTime + common::time::Milliseconds(850),
			common::time::Milliseconds(16)
		);

		controller.SetDelay(common::time::Milliseconds(120));

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(120),
			"InterpolationDelayController: manual delay overrides automatic delay"
		);
	}

	void RunResetRestoresConfiguredDefaultTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(120),
			common::time::Milliseconds(50),
			common::time::Milliseconds(300)
		);

		controller.SetDelay(common::time::Milliseconds(250));
		controller.Reset();

		tests::Expect(
			result,
			controller.GetDelay() == common::time::Milliseconds(120),
			"InterpolationDelayController: reset restores configured default"
		);
	}

	void RunInvalidTickIntervalUsesDefaultTest(tests::DebugTestResult& result)
	{
		client::game::InterpolationDelayController controller;

		controller.Configure(
			common::time::Milliseconds(0),
			common::time::Milliseconds(0),
			common::time::Milliseconds(500)
		);

		common::time::TimePoint startTime;

		controller.ObserveSnapshotTiming(
			startTime,
			startTime,
			common::time::Milliseconds::zero()
		);

		tests::Expect(
			result,
			controller.GetDelay() == common::game::defaultFixedTickInterval,
			"InterpolationDelayController: invalid tick interval uses default interval"
		);
	}
}

namespace tests::client
{
	DebugTestResult RunInterpolationDelayControllerTests()
	{
		DebugTestResult result{};

		RunConfigureTest(result);
		RunSetDelayClampTest(result);
		RunStableTimingKeepsDelayTest(result);
		RunUnderflowRiskIncreasesDelayTest(result);
		RunAutomaticIncreaseClampsToMaximumTest(result);
		RunAutomaticDecreaseWaitsForStablePeriodTest(result);
		RunAutomaticDecreaseUsesIntervalTest(result);
		RunAutomaticDecreaseStopsAtRequestedDelayTest(result);
		RunNewSpikeCancelsAutomaticDecreaseTest(result);
		RunManualSetDelayOverridesAutomaticDelayTest(result);
		RunResetRestoresConfiguredDefaultTest(result);
		RunInvalidTickIntervalUsesDefaultTest(result);

		return result;
	}
}