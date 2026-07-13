#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <Common/Net/Reliable/ReliableUdpSession.h>
#include <Common/Packet/PacketBuffer.h>

namespace tests::net
{
	class ReliableUdpVirtualNetwork
	{
	public:
		using TimePoint = common::net::ReliableUdpSession::TimePoint;
		using Duration = common::net::ReliableUdpSession::Duration;
		using PacketBuffer = common::packet::PacketBuffer;

	public:
		enum class Endpoint
		{
			Client,
			Server,
		};

		struct Config
		{
		public:
			std::size_t dropModulo = 0;
			std::size_t duplicateModulo = 0;
			std::size_t delayModulo = 0;
			std::size_t reorderModulo = 0;

			Duration delay = Duration::zero();
			Duration reorderDelay = Duration::zero();
		};

		struct Packet
		{
		public:
			Endpoint sourceEndpoint = Endpoint::Client;
			Endpoint destinationEndpoint = Endpoint::Server;
			PacketBuffer packetBuffer;
			TimePoint releaseTime;
			std::uint64_t sequence = 0;
		};

	public:
		using PacketList = std::vector<Packet>;

	private:
		Config config_;
		PacketList pendingPacketList_;

		std::uint64_t nextDecisionSequence_ = 1;
		std::uint64_t nextQueuedPacketSequence_ = 1;

		std::uint64_t submittedPacketCount_ = 0;
		std::uint64_t droppedPacketCount_ = 0;
		std::uint64_t duplicatedPacketCount_ = 0;

	public:
		ReliableUdpVirtualNetwork() = default;
		~ReliableUdpVirtualNetwork() noexcept = default;

		ReliableUdpVirtualNetwork(const ReliableUdpVirtualNetwork&) = delete;
		ReliableUdpVirtualNetwork& operator=(const ReliableUdpVirtualNetwork&) = delete;

		ReliableUdpVirtualNetwork(ReliableUdpVirtualNetwork&&) noexcept = default;
		ReliableUdpVirtualNetwork& operator=(ReliableUdpVirtualNetwork&&) noexcept = default;

	private:
		[[nodiscard]] static Endpoint GetOppositeEndpoint(Endpoint endpoint) noexcept;

	public:
		void Clear() noexcept;
		void SetConfig(const Config& config) noexcept;

		void Submit(
			Endpoint sourceEndpoint,
			const PacketBuffer& packetBuffer,
			TimePoint currentTime
		);

		[[nodiscard]] PacketList ExtractReadyPackets(
			Endpoint destinationEndpoint,
			TimePoint currentTime
		);

	private:
		void PushPacket(
			Endpoint sourceEndpoint,
			Endpoint destinationEndpoint,
			const PacketBuffer& packetBuffer,
			TimePoint releaseTime
		);

		[[nodiscard]] bool ShouldDrop(std::uint64_t decisionSequence) const noexcept;
		[[nodiscard]] bool ShouldDuplicate(std::uint64_t decisionSequence) const noexcept;
		[[nodiscard]] bool ShouldDelay(std::uint64_t decisionSequence) const noexcept;
		[[nodiscard]] bool ShouldReorder(std::uint64_t decisionSequence) const noexcept;

	public:
		[[nodiscard]] std::size_t GetPendingPacketCount() const noexcept
		{
			return pendingPacketList_.size();
		}

		[[nodiscard]] std::uint64_t GetSubmittedPacketCount() const noexcept
		{
			return submittedPacketCount_;
		}

		[[nodiscard]] std::uint64_t GetDroppedPacketCount() const noexcept
		{
			return droppedPacketCount_;
		}

		[[nodiscard]] std::uint64_t GetDuplicatedPacketCount() const noexcept
		{
			return duplicatedPacketCount_;
		}
	};
}