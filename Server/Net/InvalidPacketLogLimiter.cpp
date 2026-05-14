#include "InvalidPacketLogLimiter.h"

namespace server::net
{
	InvalidPacketLogLimiter::LogDecision InvalidPacketLogLimiter::Record(DispatchStatus status, TimePoint currentTime)
	{
		if (status == DispatchStatus::Succeeded || status == DispatchStatus::Count)
		{
			return {};
		}

		const std::size_t statusIndex = GetStatusIndex(status);
		if (statusIndex >= statusStateList_.size())
		{
			return {};
		}

		std::scoped_lock lock(mutex_);

		StatusState& statusState = statusStateList_[statusIndex];
		++statusState.totalCount;

		if (statusState.totalCount <= immediateLogCount || currentTime >= statusState.nextLogTime)
		{
			LogDecision logDecision{};
			logDecision.shouldLog = true;
			logDecision.totalCount = statusState.totalCount;
			logDecision.suppressedCount = statusState.suppressedCount;

			statusState.suppressedCount = 0;
			statusState.nextLogTime = currentTime + logInterval;

			return logDecision;
		}

		++statusState.suppressedCount;

		LogDecision logDecision{};
		logDecision.shouldLog = false;
		logDecision.totalCount = statusState.totalCount;
		logDecision.suppressedCount = statusState.suppressedCount;
		return logDecision;
	}

	void InvalidPacketLogLimiter::Reset() noexcept
	{
		std::scoped_lock lock(mutex_);

		for (StatusState& statusState : statusStateList_)
		{
			statusState = {};
		}
	}

	std::size_t InvalidPacketLogLimiter::GetStatusIndex(DispatchStatus status) noexcept
	{
		return static_cast<std::size_t>(status);
	}

	std::uint64_t InvalidPacketLogLimiter::GetTotalDroppedCount() const
	{
		std::scoped_lock lock(mutex_);

		std::uint64_t totalDroppedCount = 0;

		for (const StatusState& statusState : statusStateList_)
		{
			totalDroppedCount += statusState.totalCount;
		}

		return totalDroppedCount;
	}
}