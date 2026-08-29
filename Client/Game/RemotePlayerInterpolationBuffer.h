#pragma once

#include <cstddef>
#include <deque>
#include <optional>

#include <Common/Time/TimeTypes.h>

namespace client::game
{
	class RemotePlayerInterpolationBuffer
	{
	public:
		struct InterpolatedPosition
		{
			float x = 0.0F;
			float y = 0.0F;
		};

	private:
		struct SnapshotSample
		{
			float x = 0.0F;
			float y = 0.0F;
			common::time::TimePoint time{};
		};

	private:
		using SnapshotSampleList = std::deque<SnapshotSample>;

	private:
		static inline constexpr std::size_t maxSnapshotSampleCount = 32;

	private:
		SnapshotSampleList snapshotSampleList_;

	public:
		RemotePlayerInterpolationBuffer() = default;
		~RemotePlayerInterpolationBuffer() noexcept = default;

		RemotePlayerInterpolationBuffer(const RemotePlayerInterpolationBuffer&) = delete;
		RemotePlayerInterpolationBuffer& operator=(const RemotePlayerInterpolationBuffer&) = delete;

		RemotePlayerInterpolationBuffer(RemotePlayerInterpolationBuffer&&) = delete;
		RemotePlayerInterpolationBuffer& operator=(RemotePlayerInterpolationBuffer&&) = delete;

	private:
		[[nodiscard]] static float Lerp(float startValue, float endValue, float alpha) noexcept;

	public:
		void Clear() noexcept;
		void Reset(float x, float y, common::time::TimePoint sampleTime);
		void PushSample(float x, float y, common::time::TimePoint sampleTime);

		[[nodiscard]] std::optional<InterpolatedPosition> Interpolate(common::time::TimePoint interpolationTargetTime) const noexcept;
	};
}