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
		RunResetRestoresConfiguredDefaultTest(result);
		RunInvalidTickIntervalUsesDefaultTest(result);

		return result;
	}
}