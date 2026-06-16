#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <Common/Net/ReliableUdpSession.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Packet/PacketType.h>

#include <Tests/Net/ReliableUdpVirtualNetwork.h>

namespace tests::net::reliableUdpLoadTest
{
	struct SimulatedPeer
	{
	public:
		common::net::ReliableUdpSession session;
		std::uint64_t receivedNewDataPacketCount = 0;
		std::uint64_t receivedDuplicateDataPacketCount = 0;
		std::uint64_t receivedAckOnlyPacketCount = 0;
	};

	struct SimulatedPeerPair
	{
	public:
		SimulatedPeer client;
		SimulatedPeer server;
	};

	struct ReceiveResult
	{
	public:
		bool parsed = false;
		bool isAckOnly = false;
		bool isNewDataPacket = false;
		common::packet::PacketType packetType = common::packet::PacketType::None;
		std::optional<common::packet::PacketBuffer> ackPacketBuffer;
	};

	using PeerPairList = std::vector<SimulatedPeerPair>;
	using TimePoint = common::net::ReliableUdpSession::TimePoint;

	using PacketBufferList = std::vector<common::packet::PacketBuffer>;

	static inline constexpr std::size_t maxPendingPacketCount = 4096;
	static inline constexpr int maxResendCount = 8;
	static inline constexpr std::chrono::milliseconds resendInterval = std::chrono::milliseconds(10);

	[[nodiscard]] common::packet::ConstPacketSpan MakeConstPacketSpan(
		const common::packet::PacketBuffer& packetBuffer
	) noexcept;

	void ConfigurePeer(SimulatedPeer& peer) noexcept;

	[[nodiscard]] PeerPairList CreatePeerPairList(std::size_t peerPairCount);

	[[nodiscard]] std::optional<common::packet::PacketBuffer> SerializeJoinRoomRequest(
		std::int32_t roomId
	);

	[[nodiscard]] std::optional<common::packet::PacketBuffer> SerializeJoinRoomResponse(
		std::int32_t roomId,
		float spawnX,
		float spawnY
	);

	[[nodiscard]] std::optional<common::packet::PacketBuffer> BuildReliableDataPacket(
		SimulatedPeer& sender,
		common::packet::ConstPacketSpan serializedGamePacket,
		TimePoint currentTime
	);

	[[nodiscard]] std::optional<common::packet::PacketBuffer> BuildReliableAckPacket(
		SimulatedPeer& sender
	);

	[[nodiscard]] ReceiveResult ReceiveReliablePacket(
		SimulatedPeer& receiver,
		const common::packet::PacketBuffer& packetBuffer
	);

	[[nodiscard]] bool DeliverAckIfExists(
		SimulatedPeer& receiver,
		const std::optional<common::packet::PacketBuffer>& ackPacketBuffer
	);

	[[nodiscard]] bool HasNoPendingPackets(
		const PeerPairList& peerPairList
	) noexcept;

	[[nodiscard]] bool HasNoVirtualNetworkPendingPackets(
		const std::vector<ReliableUdpVirtualNetwork>& virtualNetworkList
	) noexcept;

	[[nodiscard]] std::size_t GetMaxClientPendingPacketCount(
		const PeerPairList& peerPairList
	) noexcept;

	[[nodiscard]] std::uint64_t GetTotalClientNewDataPacketCount(
		const PeerPairList& peerPairList
	) noexcept;

	[[nodiscard]] std::uint64_t GetTotalServerNewDataPacketCount(
		const PeerPairList& peerPairList
	) noexcept;

	[[nodiscard]] std::uint64_t GetTotalServerDuplicateDataPacketCount(
		const PeerPairList& peerPairList
	) noexcept;
}