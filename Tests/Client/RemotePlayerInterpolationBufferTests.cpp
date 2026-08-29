#include "RemotePlayerInterpolationBufferTests.h"

#include <cmath>

#include <Common/Time/TimeTypes.h>

#include <Client/Game/RemotePlayerInterpolationBuffer.h>

namespace
{
	inline constexpr float floatTolerance = 0.001F;

	[[nodiscard]] bool IsNearlyEqual(float lhs, float rhs) noexcept
	{
		return std::abs(lhs - rhs) <= floatTolerance;
	}

	void RunEmptyBufferTest(tests::DebugTestResult& result)
	{
		client::game::RemotePlayerInterpolationBuffer interpolationBuffer;

		common::time::TimePoint sampleTime;

		const auto interpolatedPosition = interpolationBuffer.Interpolate(sampleTime);

		tests::Expect(
			result,
			!interpolatedPosition.has_value(),
			"RemotePlayerInterpolationBuffer: empty buffer returns no position"
		);
	}

	void RunSingleSampleTest(tests::DebugTestResult& result)
	{
		client::game::RemotePlayerInterpolationBuffer interpolationBuffer;

		common::time::TimePoint sampleTime;

		interpolationBuffer.Reset(100.0F, 200.0F, sampleTime);

		const auto interpolatedPosition =
			interpolationBuffer.Interpolate(sampleTime + common::time::Milliseconds(100));

		tests::Expect(
			result,
			interpolatedPosition.has_value(),
			"RemotePlayerInterpolationBuffer: single sample returns position"
		);

		if (!interpolatedPosition.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			IsNearlyEqual(interpolatedPosition->x, 100.0F),
			"RemotePlayerInterpolationBuffer: single sample x"
		);

		tests::Expect(
			result,
			IsNearlyEqual(interpolatedPosition->y, 200.0F),
			"RemotePlayerInterpolationBuffer: single sample y"
		);
	}

	void RunMidpointInterpolationTest(tests::DebugTestResult& result)
	{
		client::game::RemotePlayerInterpolationBuffer interpolationBuffer;

		common::time::TimePoint startTime;

		interpolationBuffer.Reset(100.0F, 200.0F, startTime);
		interpolationBuffer.PushSample(
			200.0F,
			300.0F,
			startTime + common::time::Milliseconds(100)
		);

		const auto interpolatedPosition =
			interpolationBuffer.Interpolate(startTime + common::time::Milliseconds(50));

		tests::Expect(
			result,
			interpolatedPosition.has_value(),
			"RemotePlayerInterpolationBuffer: midpoint interpolation returns position"
		);

		if (!interpolatedPosition.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			IsNearlyEqual(interpolatedPosition->x, 150.0F),
			"RemotePlayerInterpolationBuffer: midpoint interpolation x"
		);

		tests::Expect(
			result,
			IsNearlyEqual(interpolatedPosition->y, 250.0F),
			"RemotePlayerInterpolationBuffer: midpoint interpolation y"
		);
	}

	void RunMultipleSampleIntervalTest(tests::DebugTestResult& result)
	{
		client::game::RemotePlayerInterpolationBuffer interpolationBuffer;

		common::time::TimePoint startTime;

		interpolationBuffer.Reset(100.0F, 100.0F, startTime);
		interpolationBuffer.PushSample(200.0F, 100.0F, startTime + common::time::Milliseconds(100));
		interpolationBuffer.PushSample(300.0F, 100.0F, startTime + common::time::Milliseconds(200));

		const auto interpolatedPosition =
			interpolationBuffer.Interpolate(startTime + common::time::Milliseconds(150));

		tests::Expect(
			result,
			interpolatedPosition.has_value(),
			"RemotePlayerInterpolationBuffer: multiple sample interpolation returns position"
		);

		if (!interpolatedPosition.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			IsNearlyEqual(interpolatedPosition->x, 250.0F),
			"RemotePlayerInterpolationBuffer: correct sample interval selected"
		);
	}

	void RunBeforeOldestSampleTest(tests::DebugTestResult& result)
	{
		client::game::RemotePlayerInterpolationBuffer interpolationBuffer;

		common::time::TimePoint startTime;

		interpolationBuffer.Reset(100.0F, 200.0F, startTime);

		const auto interpolatedPosition =
			interpolationBuffer.Interpolate(startTime - common::time::Milliseconds(100));

		tests::Expect(
			result,
			interpolatedPosition.has_value() && IsNearlyEqual(interpolatedPosition->x, 100.0F),
			"RemotePlayerInterpolationBuffer: target before oldest sample uses oldest position"
		);
	}

	void RunAfterLatestSampleTest(tests::DebugTestResult& result)
	{
		client::game::RemotePlayerInterpolationBuffer interpolationBuffer;

		common::time::TimePoint startTime;

		interpolationBuffer.Reset(100.0F, 200.0F, startTime);
		interpolationBuffer.PushSample(200.0F, 300.0F, startTime + common::time::Milliseconds(100));

		const auto interpolatedPosition =
			interpolationBuffer.Interpolate(startTime + common::time::Milliseconds(500));

		tests::Expect(
			result,
			interpolatedPosition.has_value() && IsNearlyEqual(interpolatedPosition->x, 200.0F),
			"RemotePlayerInterpolationBuffer: target after latest sample uses latest position"
		);
	}

	void RunEqualTimestampReplacesSampleTest(tests::DebugTestResult& result)
	{
		client::game::RemotePlayerInterpolationBuffer interpolationBuffer;

		common::time::TimePoint sampleTime;

		interpolationBuffer.Reset(100.0F, 200.0F, sampleTime);
		interpolationBuffer.PushSample(300.0F, 400.0F, sampleTime);

		const auto interpolatedPosition = interpolationBuffer.Interpolate(sampleTime);

		tests::Expect(
			result,
			interpolatedPosition.has_value() && IsNearlyEqual(interpolatedPosition->x, 300.0F),
			"RemotePlayerInterpolationBuffer: equal timestamp replaces latest sample"
		);
	}

	void RunOlderTimestampIgnoredTest(tests::DebugTestResult& result)
	{
		client::game::RemotePlayerInterpolationBuffer interpolationBuffer;

		common::time::TimePoint startTime;

		interpolationBuffer.Reset(100.0F, 200.0F, startTime);
		interpolationBuffer.PushSample(200.0F, 300.0F, startTime + common::time::Milliseconds(100));
		interpolationBuffer.PushSample(999.0F, 999.0F, startTime + common::time::Milliseconds(50));

		const auto interpolatedPosition =
			interpolationBuffer.Interpolate(startTime + common::time::Milliseconds(100));

		tests::Expect(
			result,
			interpolatedPosition.has_value() && IsNearlyEqual(interpolatedPosition->x, 200.0F),
			"RemotePlayerInterpolationBuffer: older timestamp ignored"
		);
	}
}

namespace tests::client
{
	DebugTestResult RunRemotePlayerInterpolationBufferTests()
	{
		DebugTestResult result{};

		RunEmptyBufferTest(result);
		RunSingleSampleTest(result);
		RunMidpointInterpolationTest(result);
		RunMultipleSampleIntervalTest(result);
		RunBeforeOldestSampleTest(result);
		RunAfterLatestSampleTest(result);
		RunEqualTimestampReplacesSampleTest(result);
		RunOlderTimestampIgnoredTest(result);

		return result;
	}
}