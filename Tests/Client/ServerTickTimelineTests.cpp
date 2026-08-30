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

		const common::time::TimePoint sampleTime =
			timeline.ResolveSampleTime(
				100,
				common::time::Milliseconds(16),
				startTime + common::time::Milliseconds(1000)
			);

		tests::Expect(
			result,
			sampleTime == startTime + common::time::Milliseconds(1000),
			"ServerTickTimeline: first snapshot anchors arrival time"
		);
	}

	void RunArrivalJitterDoesNotChangeSampleSpacingTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		const common::time::TimePoint firstSampleTime =
			timeline.ResolveSampleTime(
				100,
				common::time::Milliseconds(16),
				startTime + common::time::Milliseconds(1000)
			);

		const common::time::TimePoint secondSampleTime =
			timeline.ResolveSampleTime(
				101,
				common::time::Milliseconds(16),
				startTime + common::time::Milliseconds(1080)
			);

		const common::time::TimePoint thirdSampleTime =
			timeline.ResolveSampleTime(
				102,
				common::time::Milliseconds(16),
				startTime + common::time::Milliseconds(1090)
			);

		tests::Expect(
			result,
			secondSampleTime - firstSampleTime == common::time::Milliseconds(16),
			"ServerTickTimeline: jitter does not change second sample spacing"
		);

		tests::Expect(
			result,
			thirdSampleTime - secondSampleTime == common::time::Milliseconds(16),
			"ServerTickTimeline: jitter does not change third sample spacing"
		);
	}

	void RunSkippedServerTicksPreserveSimulationTimeTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		const common::time::TimePoint firstSampleTime =
			timeline.ResolveSampleTime(100, common::time::Milliseconds(16), startTime);

		const common::time::TimePoint skippedSampleTime =
			timeline.ResolveSampleTime(
				105,
				common::time::Milliseconds(16),
				startTime + common::time::Milliseconds(200)
			);

		tests::Expect(
			result,
			skippedSampleTime - firstSampleTime == common::time::Milliseconds(80),
			"ServerTickTimeline: skipped server ticks preserve simulation time"
		);
	}

	void RunWrappedServerTickTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		constexpr std::uint32_t maxServerTick = std::numeric_limits<std::uint32_t>::max();

		const common::time::TimePoint maxTickSampleTime =
			timeline.ResolveSampleTime(maxServerTick, common::time::Milliseconds(16), startTime);

		const common::time::TimePoint wrappedSampleTime =
			timeline.ResolveSampleTime(
				0,
				common::time::Milliseconds(16),
				startTime + common::time::Milliseconds(100)
			);

		tests::Expect(
			result,
			wrappedSampleTime - maxTickSampleTime == common::time::Milliseconds(16),
			"ServerTickTimeline: wrapped server tick advances one interval"
		);
	}

	void RunTickIntervalChangeReanchorsTimelineTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		static_cast<void>(
			timeline.ResolveSampleTime(100, common::time::Milliseconds(16), startTime)
			);

		const common::time::TimePoint reanchoredSampleTime =
			timeline.ResolveSampleTime(
				101,
				common::time::Milliseconds(20),
				startTime + common::time::Milliseconds(300)
			);

		tests::Expect(
			result,
			reanchoredSampleTime == startTime + common::time::Milliseconds(300),
			"ServerTickTimeline: tick interval change reanchors timeline"
		);

		const common::time::TimePoint nextSampleTime =
			timeline.ResolveSampleTime(
				102,
				common::time::Milliseconds(20),
				startTime + common::time::Milliseconds(500)
			);

		tests::Expect(
			result,
			nextSampleTime == startTime + common::time::Milliseconds(320),
			"ServerTickTimeline: reanchored timeline uses new interval"
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

		const common::time::TimePoint sampleTime =
			timeline.ResolveSampleTime(
				500,
				common::time::Milliseconds(16),
				startTime + common::time::Milliseconds(1000)
			);

		tests::Expect(
			result,
			sampleTime == startTime + common::time::Milliseconds(1000),
			"ServerTickTimeline: clear resets timeline anchor"
		);
	}

	void RunInvalidIntervalUsesDefaultTest(tests::DebugTestResult& result)
	{
		client::game::ServerTickTimeline timeline;

		common::time::TimePoint startTime;

		const common::time::TimePoint firstSampleTime =
			timeline.ResolveSampleTime(100, common::time::Milliseconds(0), startTime);

		const common::time::TimePoint secondSampleTime =
			timeline.ResolveSampleTime(
				101,
				common::time::Milliseconds(0),
				startTime + common::time::Milliseconds(100)
			);

		tests::Expect(
			result,
			secondSampleTime - firstSampleTime == common::time::Milliseconds(50),
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
		RunClearReanchorsTimelineTest(result);
		RunInvalidIntervalUsesDefaultTest(result);

		return result;
	}
}