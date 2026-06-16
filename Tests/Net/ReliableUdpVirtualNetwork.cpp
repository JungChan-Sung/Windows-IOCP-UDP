#include "ReliableUdpVirtualNetwork.h"

#include <algorithm>
#include <utility>

namespace tests::net
{
	ReliableUdpVirtualNetwork::Endpoint ReliableUdpVirtualNetwork::GetOppositeEndpoint(
		Endpoint endpoint
	) noexcept
	{
		if (endpoint == Endpoint::Client)
		{
			return Endpoint::Server;
		}

		return Endpoint::Client;
	}

	void ReliableUdpVirtualNetwork::Clear() noexcept
	{
		pendingPacketList_.clear();

		nextDecisionSequence_ = 1;
		nextQueuedPacketSequence_ = 1;

		submittedPacketCount_ = 0;
		droppedPacketCount_ = 0;
		duplicatedPacketCount_ = 0;
	}

	void ReliableUdpVirtualNetwork::SetConfig(const Config& config) noexcept
	{
		config_ = config;
	}

	void ReliableUdpVirtualNetwork::Submit(
		Endpoint sourceEndpoint,
		const PacketBuffer& packetBuffer,
		TimePoint currentTime
	)
	{
		++submittedPacketCount_;

		const std::uint64_t decisionSequence = nextDecisionSequence_;
		++nextDecisionSequence_;

		if (ShouldDrop(decisionSequence))
		{
			++droppedPacketCount_;
			return;
		}

		Duration delay = Duration::zero();

		if (ShouldDelay(decisionSequence))
		{
			delay += config_.delay;
		}

		if (ShouldReorder(decisionSequence))
		{
			delay += config_.reorderDelay;
		}

		const Endpoint destinationEndpoint = GetOppositeEndpoint(sourceEndpoint);
		const TimePoint releaseTime = currentTime + delay;

		PushPacket(
			sourceEndpoint,
			destinationEndpoint,
			packetBuffer,
			releaseTime
		);

		if (ShouldDuplicate(decisionSequence))
		{
			++duplicatedPacketCount_;

			PushPacket(
				sourceEndpoint,
				destinationEndpoint,
				packetBuffer,
				releaseTime
			);
		}
	}

	ReliableUdpVirtualNetwork::PacketList ReliableUdpVirtualNetwork::ExtractReadyPackets(
		Endpoint destinationEndpoint,
		TimePoint currentTime
	)
	{
		PacketList readyPacketList;

		for (auto packetIterator = pendingPacketList_.begin();
			packetIterator != pendingPacketList_.end();)
		{
			if (packetIterator->destinationEndpoint != destinationEndpoint
				|| currentTime < packetIterator->releaseTime)
			{
				++packetIterator;
				continue;
			}

			readyPacketList.push_back(std::move(*packetIterator));
			packetIterator = pendingPacketList_.erase(packetIterator);
		}

		std::sort(
			readyPacketList.begin(),
			readyPacketList.end(),
			[](const Packet& lhs, const Packet& rhs)
			{
				if (lhs.releaseTime != rhs.releaseTime)
				{
					return lhs.releaseTime < rhs.releaseTime;
				}

				return lhs.sequence < rhs.sequence;
			}
		);

		return readyPacketList;
	}

	void ReliableUdpVirtualNetwork::PushPacket(
		Endpoint sourceEndpoint,
		Endpoint destinationEndpoint,
		const PacketBuffer& packetBuffer,
		TimePoint releaseTime
	)
	{
		Packet packet{};
		packet.sourceEndpoint = sourceEndpoint;
		packet.destinationEndpoint = destinationEndpoint;
		packet.packetBuffer = packetBuffer;
		packet.releaseTime = releaseTime;
		packet.sequence = nextQueuedPacketSequence_;

		++nextQueuedPacketSequence_;

		pendingPacketList_.push_back(std::move(packet));
	}

	bool ReliableUdpVirtualNetwork::ShouldDrop(
		std::uint64_t decisionSequence
	) const noexcept
	{
		return config_.dropModulo != 0
			&& decisionSequence % config_.dropModulo == 0;
	}

	bool ReliableUdpVirtualNetwork::ShouldDuplicate(
		std::uint64_t decisionSequence
	) const noexcept
	{
		return config_.duplicateModulo != 0
			&& decisionSequence % config_.duplicateModulo == 0;
	}

	bool ReliableUdpVirtualNetwork::ShouldDelay(
		std::uint64_t decisionSequence
	) const noexcept
	{
		return config_.delayModulo != 0
			&& decisionSequence % config_.delayModulo == 0;
	}

	bool ReliableUdpVirtualNetwork::ShouldReorder(
		std::uint64_t decisionSequence
	) const noexcept
	{
		return config_.reorderModulo != 0
			&& decisionSequence % config_.reorderModulo == 0;
	}
}