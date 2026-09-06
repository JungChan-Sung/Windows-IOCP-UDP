#include "RemotePlayerInterpolationBuffer.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace client::game
{
	void RemotePlayerInterpolationBuffer::Clear() noexcept
	{
		snapshotSampleList_.clear();
	}

	void RemotePlayerInterpolationBuffer::Reset(float x, float y, common::time::TimePoint sampleTime)
	{
		snapshotSampleList_.clear();

		SnapshotSample snapshotSample{};
		snapshotSample.x = x;
		snapshotSample.y = y;
		snapshotSample.time = sampleTime;

		snapshotSampleList_.push_back(snapshotSample);
	}

	void RemotePlayerInterpolationBuffer::PushSample(float x, float y, common::time::TimePoint sampleTime)
	{
		if (!snapshotSampleList_.empty())
		{
			SnapshotSample& latestSample = snapshotSampleList_.back();
			if (sampleTime < latestSample.time)
			{
				return;
			}

			if (sampleTime == latestSample.time)
			{
				latestSample.x = x;
				latestSample.y = y;
				return;
			}
		}

		SnapshotSample snapshotSample{};
		snapshotSample.x = x;
		snapshotSample.y = y;
		snapshotSample.time = sampleTime;

		snapshotSampleList_.push_back(snapshotSample);

		if (snapshotSampleList_.size() > maxSnapshotSampleCount)
		{
			snapshotSampleList_.pop_front();
		}
	}

	void RemotePlayerInterpolationBuffer::PruneBefore(common::time::TimePoint minimumInterpolationTargetTime) noexcept
	{
		while (snapshotSampleList_.size() >= 2 && snapshotSampleList_[1].time <= minimumInterpolationTargetTime)
		{
			snapshotSampleList_.pop_front();
		}
	}

	std::optional<RemotePlayerInterpolationBuffer::InterpolatedPosition> RemotePlayerInterpolationBuffer::Interpolate(common::time::TimePoint interpolationTargetTime) const noexcept
	{
		if (snapshotSampleList_.empty())
		{
			return std::nullopt;
		}

		if (interpolationTargetTime <= snapshotSampleList_.front().time)
		{
			return InterpolatedPosition{
				.x = snapshotSampleList_.front().x,
				.y = snapshotSampleList_.front().y,
			};
		}

		if (interpolationTargetTime >= snapshotSampleList_.back().time)
		{
			return InterpolatedPosition{
				.x = snapshotSampleList_.back().x,
				.y = snapshotSampleList_.back().y,
			};
		}

		const auto targetSampleIterator = std::ranges::lower_bound(snapshotSampleList_, interpolationTargetTime, {}, &SnapshotSample::time);
		const auto previousSampleIterator = std::prev(targetSampleIterator);
		const float totalSeconds = common::time::FloatSeconds(targetSampleIterator->time - previousSampleIterator->time).count();
		if (totalSeconds <= 0.0F)
		{
			return InterpolatedPosition{
				.x = targetSampleIterator->x,
				.y = targetSampleIterator->y,
			};
		}

		const float elapsedSeconds = common::time::FloatSeconds(interpolationTargetTime - previousSampleIterator->time).count();
		const float alpha = std::clamp(elapsedSeconds / totalSeconds, 0.0F, 1.0F);

		return InterpolatedPosition{
			.x = std::lerp(previousSampleIterator->x, targetSampleIterator->x, alpha),
			.y = std::lerp(previousSampleIterator->y, targetSampleIterator->y, alpha),
		};
	}
	std::optional<RemotePlayerInterpolationBuffer::InterpolatedPosition> RemotePlayerInterpolationBuffer::GetLatestPosition() const noexcept
	{
		if (snapshotSampleList_.empty())
		{
			return std::nullopt;
		}

		return InterpolatedPosition{
			.x = snapshotSampleList_.back().x,
			.y = snapshotSampleList_.back().y,
		};
	}
}