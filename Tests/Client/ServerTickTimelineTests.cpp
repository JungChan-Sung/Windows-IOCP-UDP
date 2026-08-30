#include "ServerTickTimelineTests.h"

#include <cstdint>
#include <limits>

#include <Common/Time/TimeTypes.h>

#include <Client/Game/ServerTickTimeline.h>

namespace
{
	void RunFirstSnapshotAnchorsArrivalTimeTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		const auto sampleTimeResult = timeline.ResolveSampleTime(
			100,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1000)
		);

		tests::Expect(
			result,
			sampleTimeResult.sampleTime == startTime + common::time::Milliseconds(1000),
			"ServerTickTimeline: first snapshot anchors arrival time"
		);

		tests::Expect(
			result,
			!sampleTimeResult.wasReanchored,
			"ServerTickTimeline: first snapshot is not reported as reanchor"
		);
	}

	void RunArrivalJitterDoesNotChangeSampleSpacingTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		const auto firstResult = timeline.ResolveSampleTime(
			100,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1000)
		);

		const auto secondResult = timeline.ResolveSampleTime(
			101,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1080)
		);

		const auto thirdResult = timeline.ResolveSampleTime(
			102,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1090)
		);

		tests::Expect(
			result,
			secondResult.sampleTime - firstResult.sampleTime == common::time::Milliseconds(16),
			"ServerTickTimeline: jitter does not change second sample spacing"
		);

		tests::Expect(
			result,
			thirdResult.sampleTime - secondResult.sampleTime == common::time::Milliseconds(16),
			"ServerTickTimeline: jitter does not change third sample spacing"
		);

		tests::Expect(
			result,
			!secondResult.wasReanchored && !thirdResult.wasReanchored,
			"ServerTickTimeline: ordinary jitter does not reanchor"
		);
	}

	void RunSkippedServerTicksPreserveSimulationTimeTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		const auto firstResult =
			timeline.ResolveSampleTime(100, common::time::Milliseconds(16), startTime);

		const auto skippedResult = timeline.ResolveSampleTime(
			105,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(200)
		);

		tests::Expect(
			result,
			skippedResult.sampleTime - firstResult.sampleTime == common::time::Milliseconds(80),
			"ServerTickTimeline: skipped server ticks preserve simulation time"
		);

		tests::Expect(
			result,
			!skippedResult.wasReanchored,
			"ServerTickTimeline: single skipped snapshot does not reanchor"
		);
	}

	void RunWrappedServerTickTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		constexpr std::uint32_t maxServerTick = std::numeric_limits<std::uint32_t>::max();

		const auto maxTickResult =
			timeline.ResolveSampleTime(maxServerTick, common::time::Milliseconds(16), startTime);

		const auto wrappedResult = timeline.ResolveSampleTime(
			0,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(100)
		);

		tests::Expect(
			result,
			wrappedResult.sampleTime - maxTickResult.sampleTime == common::time::Milliseconds(16),
			"ServerTickTimeline: wrapped server tick advances one interval"
		);

		tests::Expect(
			result,
			!wrappedResult.wasReanchored,
			"ServerTickTimeline: wrapped server tick does not reanchor"
		);
	}

	void RunTickIntervalChangeReanchorsTimelineTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		static_cast<void>(
			timeline.ResolveSampleTime(100, common::time::Milliseconds(16), startTime)
			);

		const auto reanchoredResult = timeline.ResolveSampleTime(
			101,
			common::time::Milliseconds(20),
			startTime + common::time::Milliseconds(300)
		);

		tests::Expect(
			result,
			reanchoredResult.sampleTime == startTime + common::time::Milliseconds(300),
			"ServerTickTimeline: tick interval change reanchors timeline"
		);

		tests::Expect(
			result,
			reanchoredResult.wasReanchored,
			"ServerTickTimeline: tick interval change reports reanchor"
		);

		const auto nextResult = timeline.ResolveSampleTime(
			102,
			common::time::Milliseconds(20),
			startTime + common::time::Milliseconds(500)
		);

		tests::Expect(
			result,
			nextResult.sampleTime == startTime + common::time::Milliseconds(320),
			"ServerTickTimeline: reanchored timeline uses new interval"
		);

		tests::Expect(
			result,
			!nextResult.wasReanchored,
			"ServerTickTimeline: normal sample after interval change is not reanchor"
		);
	}

	void RunPersistentDriftReanchorsTimelineTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		const auto firstResult = timeline.ResolveSampleTime(
			100,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1000)
		);

		const auto secondResult = timeline.ResolveSampleTime(
			101,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1200)
		);

		const auto thirdResult = timeline.ResolveSampleTime(
			102,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1216)
		);

		const auto fourthResult = timeline.ResolveSampleTime(
			103,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1232)
		);

		const auto reanchoredResult = timeline.ResolveSampleTime(
			104,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1248)
		);

		tests::Expect(
			result,
			firstResult.sampleTime == startTime + common::time::Milliseconds(1000),
			"ServerTickTimeline: persistent drift first sample"
		);

		tests::Expect(
			result,
			secondResult.sampleTime == startTime + common::time::Milliseconds(1016)
			&& !secondResult.wasReanchored,
			"ServerTickTimeline: first drift observation does not reanchor"
		);

		tests::Expect(
			result,
			thirdResult.sampleTime == startTime + common::time::Milliseconds(1032)
			&& !thirdResult.wasReanchored,
			"ServerTickTimeline: second drift observation does not reanchor"
		);

		tests::Expect(
			result,
			fourthResult.sampleTime == startTime + common::time::Milliseconds(1048)
			&& !fourthResult.wasReanchored,
			"ServerTickTimeline: third drift observation does not reanchor"
		);

		tests::Expect(
			result,
			reanchoredResult.sampleTime == startTime + common::time::Milliseconds(1248),
			"ServerTickTimeline: persistent drift reanchors after confirmation"
		);

		tests::Expect(
			result,
			reanchoredResult.wasReanchored,
			"ServerTickTimeline: persistent drift reports reanchor"
		);

		const auto nextResult = timeline.ResolveSampleTime(
			105,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1264)
		);

		tests::Expect(
			result,
			nextResult.sampleTime == startTime + common::time::Milliseconds(1264),
			"ServerTickTimeline: new anchor restores tick spacing"
		);

		tests::Expect(
			result,
			!nextResult.wasReanchored,
			"ServerTickTimeline: sample after persistent reanchor is normal"
		);
	}

	void RunDriftConfirmationResetTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		static_cast<void>(
			timeline.ResolveSampleTime(
				100,
				common::time::Milliseconds(16),
				startTime + common::time::Milliseconds(1000)
			)
			);

		const auto firstDriftResult = timeline.ResolveSampleTime(
			101,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1120)
		);

		const auto recoveredResult = timeline.ResolveSampleTime(
			102,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1125)
		);

		const auto secondDriftResult = timeline.ResolveSampleTime(
			103,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1160)
		);

		const auto thirdDriftResult = timeline.ResolveSampleTime(
			104,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1176)
		);

		const auto fourthDriftResult = timeline.ResolveSampleTime(
			105,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1192)
		);

		tests::Expect(
			result,
			!firstDriftResult.wasReanchored,
			"ServerTickTimeline: first drift starts confirmation"
		);

		tests::Expect(
			result,
			!recoveredResult.wasReanchored,
			"ServerTickTimeline: timing recovery resets confirmation"
		);

		tests::Expect(
			result,
			!secondDriftResult.wasReanchored
			&& !thirdDriftResult.wasReanchored
			&& !fourthDriftResult.wasReanchored,
			"ServerTickTimeline: confirmation restarts after recovery"
		);

		tests::Expect(
			result,
			fourthDriftResult.sampleTime == startTime + common::time::Milliseconds(1080),
			"ServerTickTimeline: incomplete confirmation preserves original timeline"
		);
	}

	void RunClearReanchorsTimelineTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		static_cast<void>(
			timeline.ResolveSampleTime(100, common::time::Milliseconds(16), startTime)
			);

		timeline.Clear();

		const auto sampleTimeResult = timeline.ResolveSampleTime(
			500,
			common::time::Milliseconds(16),
			startTime + common::time::Milliseconds(1000)
		);

		tests::Expect(
			result,
			sampleTimeResult.sampleTime == startTime + common::time::Milliseconds(1000),
			"ServerTickTimeline: clear resets timeline anchor"
		);

		tests::Expect(
			result,
			!sampleTimeResult.wasReanchored,
			"ServerTickTimeline: first sample after clear is not reanchor"
		);
	}

	void RunInvalidIntervalUsesDefaultTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		const auto firstResult =
			timeline.ResolveSampleTime(100, common::time::Milliseconds(0), startTime);

		const auto secondResult = timeline.ResolveSampleTime(
			101,
			common::time::Milliseconds(0),
			startTime + common::time::Milliseconds(100)
		);

		tests::Expect(
			result,
			secondResult.sampleTime - firstResult.sampleTime == common::time::Milliseconds(50),
			"ServerTickTimeline: invalid interval uses default interval"
		);
	}
}

namespace tests::client
{
	DebugTestResult RunServerTickTimelineTests()
	{
		DebugTestResult result{};

		RunFirstSnapshotAnchorsArrivalTimeTest(result);
		RunArrivalJitterDoesNotChangeSampleSpacingTest(result);
		RunSkippedServerTicksPreserveSimulationTimeTest(result);
		RunWrappedServerTickTest(result);
		RunTickIntervalChangeReanchorsTimelineTest(result);
		RunPersistentDriftReanchorsTimelineTest(result);
		RunDriftConfirmationResetTest(result);
		RunClearReanchorsTimelineTest(result);
		RunInvalidIntervalUsesDefaultTest(result);

		return result;
	}
}