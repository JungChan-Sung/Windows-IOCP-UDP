#include "UdpFaultPacketScheduler.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace common::net
{
	UdpFaultPacketScheduler::SubmitResult UdpFaultPacketScheduler::Submit(const sockaddr_in& remoteAddress, common::packet::ConstPacketSpan packetData,	const Decision& decision, time::TimePoint currentTime)
	{
		SubmitResult submitResult{};

		if (decision.shouldDrop)
		{
			submitResult.dropped = true;
			return submitResult;
		}

		const std::size_t packetCount = decision.shouldDuplicate ? 2 : 1;
		const time::Duration delay = std::max(decision.delay, time::Duration::zero());

		if (delay == time::Duration::zero())
		{
			submitResult.readyPacketList.reserve(packetCount);

			for (std::size_t index = 0; index < packetCount; ++index)
			{
				submitResult.readyPacketList.push_back(CreatePacket(remoteAddress, packetData));
			}

			return submitResult;
		}

		const time::TimePoint releaseTime = currentTime + delay;

		{
			std::scoped_lock lock(schedulerMutex_);

			for (std::size_t index = 0; index < packetCount; ++index)
			{
				const ScheduleKey scheduleKey{ releaseTime, nextScheduleOrder_++ };
				pendingPacketMap_.emplace(scheduleKey, CreatePacket(remoteAddress, packetData));
			}
		}

		return submitResult;
	}

	UdpFaultPacketScheduler::PacketList UdpFaultPacketScheduler::ExtractReadyPackets(time::TimePoint currentTime)
	{
		PacketList readyPacketList;

		std::scoped_lock lock(schedulerMutex_);

		auto packetIterator = pendingPacketMap_.begin();
		while (packetIterator != pendingPacketMap_.end() && packetIterator->first.first <= currentTime)
		{
			readyPacketList.push_back(std::move(packetIterator->second));
			packetIterator = pendingPacketMap_.erase(packetIterator);
		}

		return readyPacketList;
	}

	void UdpFaultPacketScheduler::Reset() noexcept
	{
		std::scoped_lock lock(schedulerMutex_);

		pendingPacketMap_.clear();
		nextScheduleOrder_ = 0;
	}

	UdpFaultPacketScheduler::Packet UdpFaultPacketScheduler::CreatePacket(const sockaddr_in& remoteAddress, common::packet::ConstPacketSpan packetData)
	{
		Packet packet{};
		packet.remoteAddress = remoteAddress;
		packet.packetBuffer.assign(packetData.begin(), packetData.end());
		return packet;
	}

	std::size_t UdpFaultPacketScheduler::GetPendingPacketCount() const noexcept
	{
		std::scoped_lock lock(schedulerMutex_);
		return pendingPacketMap_.size();
	}
}