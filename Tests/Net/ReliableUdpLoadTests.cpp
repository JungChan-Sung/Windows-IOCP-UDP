#include "ReliableUdpLoadTests.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <Common/Net/ReliableUdpSession.h>
#include <Common/Packet/GamePacket.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Packet/ReliableUdpPacketBuilder.h>

#include <Tests/DebugTestResult.h>

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

	static inline constexpr std::size_t roundTripClientCount = 32;
	static inline constexpr std::size_t roundTripRequestCountPerClient = 128;

	static inline constexpr std::size_t resendClientCount = 16;
	static inline constexpr std::size_t resendRequestCountPerClient = 64;

	static inline constexpr std::chrono::milliseconds resendInterval = std::chrono::milliseconds(10);
	static inline constexpr std::size_t maxPendingPacketCount = 4096;
	static inline constexpr int maxResendCount = 8;

	[[nodiscard]] common::packet::ConstPacketSpan MakeConstPacketSpan(
		const common::packet::PacketBuffer& packetBuffer
	) noexcept
	{
		return common::packet::ConstPacketSpan(
			packetBuffer.data(),
			packetBuffer.size()
		);
	}

	void ConfigurePeer(SimulatedPeer& peer) noexcept
	{
		peer.session.SetMaxPendingPacketCount(maxPendingPacketCount);
		peer.session.SetMaxResendCount(maxResendCount);
		peer.session.SetResendInterval(resendInterval);
	}

	[[nodiscard]] PeerPairList CreatePeerPairList(std::size_t peerPairCount)
	{
		PeerPairList peerPairList;
		peerPairList.reserve(peerPairCount);

		for (std::size_t index = 0; index < peerPairCount; ++index)
		{
			peerPairList.emplace_back();

			ConfigurePeer(peerPairList.back().client);
			ConfigurePeer(peerPairList.back().server);
		}

		return peerPairList;
	}

	[[nodiscard]] std::optional<common::packet::PacketBuffer> SerializeJoinRoomRequest(
		std::int32_t roomId
	)
	{
		common::packet::JoinRoomRequestPacket packet{};
		packet.roomId = roomId;

		return common::packet::SerializePacket(packet);
	}

	[[nodiscard]] std::optional<common::packet::PacketBuffer> SerializeJoinRoomResponse(
		std::int32_t roomId,
		float spawnX,
		float spawnY
	)
	{
		common::packet::JoinRoomResponsePacket packet{};
		packet.roomId = roomId;
		packet.spawnX = spawnX;
		packet.spawnY = spawnY;

		return common::packet::SerializePacket(packet);
	}

	[[nodiscard]] std::optional<common::packet::PacketBuffer> BuildReliableDataPacket(
		SimulatedPeer& sender,
		common::packet::ConstPacketSpan serializedGamePacket,
		TimePoint currentTime
	)
	{
		const common::net::ReliableSequence sequence =
			sender.session.AllocateOutgoingSequence();

		const common::net::ReliableUdpPacketHeader reliableHeader =
			sender.session.BuildOutgoingHeader(sequence);

		std::optional<common::packet::PacketBuffer> reliablePacketBuffer =
			common::packet::BuildReliableUdpPacket(
				reliableHeader,
				serializedGamePacket
			);

		if (!reliablePacketBuffer.has_value())
		{
			return std::nullopt;
		}

		if (!sender.session.RegisterSentPacket(
			sequence,
			*reliablePacketBuffer,
			currentTime
		))
		{
			return std::nullopt;
		}

		return reliablePacketBuffer;
	}

	[[nodiscard]] std::optional<common::packet::PacketBuffer> BuildReliableAckPacket(
		SimulatedPeer& sender
	)
	{
		const common::net::ReliableUdpPacketHeader reliableHeader =
			sender.session.BuildOutgoingAckHeader();

		return common::packet::BuildReliableUdpAckPacket(reliableHeader);
	}

	[[nodiscard]] ReceiveResult ReceiveReliablePacket(
		SimulatedPeer& receiver,
		const common::packet::PacketBuffer& packetBuffer
	)
	{
		ReceiveResult result{};

		const std::optional<common::packet::ReliableUdpPacketView> packetView =
			common::packet::ParseReliableUdpPacket(
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		if (!packetView.has_value())
		{
			return result;
		}

		result.parsed = true;
		result.packetType = packetView->packetHeader.type;

		if (packetView->packetHeader.type == common::packet::PacketType::None)
		{
			result.isAckOnly = true;

			static_cast<void>(
				receiver.session.ProcessReceivedAck(
					packetView->reliableHeader
				)
				);

			++receiver.receivedAckOnlyPacketCount;
			return result;
		}

		result.isNewDataPacket =
			receiver.session.ProcessReceivedDataHeader(
				packetView->reliableHeader
			);

		if (result.isNewDataPacket)
		{
			++receiver.receivedNewDataPacketCount;
		}
		else
		{
			++receiver.receivedDuplicateDataPacketCount;
		}

		result.ackPacketBuffer = BuildReliableAckPacket(receiver);
		return result;
	}

	[[nodiscard]] bool DeliverAckIfExists(
		SimulatedPeer& receiver,
		const std::optional<common::packet::PacketBuffer>& ackPacketBuffer
	)
	{
		if (!ackPacketBuffer.has_value())
		{
			return false;
		}

		const ReceiveResult receiveResult =
			ReceiveReliablePacket(receiver, *ackPacketBuffer);

		return receiveResult.parsed && receiveResult.isAckOnly;
	}

	[[nodiscard]] bool HasNoPendingPackets(
		const PeerPairList& peerPairList
	) noexcept
	{
		for (const SimulatedPeerPair& peerPair : peerPairList)
		{
			if (peerPair.client.session.GetPendingPacketCount() != 0)
			{
				return false;
			}

			if (peerPair.server.session.GetPendingPacketCount() != 0)
			{
				return false;
			}
		}

		return true;
	}

	[[nodiscard]] std::uint64_t GetTotalClientNewDataPacketCount(
		const PeerPairList& peerPairList
	) noexcept
	{
		std::uint64_t count = 0;

		for (const SimulatedPeerPair& peerPair : peerPairList)
		{
			count += peerPair.client.receivedNewDataPacketCount;
		}

		return count;
	}

	[[nodiscard]] std::uint64_t GetTotalServerNewDataPacketCount(
		const PeerPairList& peerPairList
	) noexcept
	{
		std::uint64_t count = 0;

		for (const SimulatedPeerPair& peerPair : peerPairList)
		{
			count += peerPair.server.receivedNewDataPacketCount;
		}

		return count;
	}

	[[nodiscard]] std::uint64_t GetTotalServerDuplicateDataPacketCount(
		const PeerPairList& peerPairList
	) noexcept
	{
		std::uint64_t count = 0;

		for (const SimulatedPeerPair& peerPair : peerPairList)
		{
			count += peerPair.server.receivedDuplicateDataPacketCount;
		}

		return count;
	}

	void RunManyClientRoundTripLoadTest(tests::DebugTestResult& result)
	{
		PeerPairList peerPairList = CreatePeerPairList(roundTripClientCount);

		TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		std::uint64_t successfulRequestCount = 0;
		std::uint64_t successfulResponseCount = 0;
		std::uint64_t ackDeliveryCount = 0;
		std::uint64_t buildFailureCount = 0;

		for (std::size_t requestIndex = 0; requestIndex < roundTripRequestCountPerClient; ++requestIndex)
		{
			for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
			{
				SimulatedPeerPair& peerPair = peerPairList[clientIndex];

				const std::int32_t roomId =
					static_cast<std::int32_t>((requestIndex + clientIndex) % 3) + 1;

				const std::optional<common::packet::PacketBuffer> requestPayload =
					SerializeJoinRoomRequest(roomId);

				if (!requestPayload.has_value())
				{
					++buildFailureCount;
					continue;
				}

				const std::optional<common::packet::PacketBuffer> requestPacket =
					BuildReliableDataPacket(
						peerPair.client,
						MakeConstPacketSpan(*requestPayload),
						currentTime
					);

				if (!requestPacket.has_value())
				{
					++buildFailureCount;
					continue;
				}

				const ReceiveResult requestReceiveResult =
					ReceiveReliablePacket(peerPair.server, *requestPacket);

				if (requestReceiveResult.parsed && requestReceiveResult.isNewDataPacket)
				{
					++successfulRequestCount;
				}

				if (DeliverAckIfExists(peerPair.client, requestReceiveResult.ackPacketBuffer))
				{
					++ackDeliveryCount;
				}

				if (!requestReceiveResult.isNewDataPacket)
				{
					currentTime += std::chrono::milliseconds(1);
					continue;
				}

				const std::optional<common::packet::PacketBuffer> responsePayload =
					SerializeJoinRoomResponse(
						roomId,
						100.0F + static_cast<float>(clientIndex),
						200.0F + static_cast<float>(requestIndex)
					);

				if (!responsePayload.has_value())
				{
					++buildFailureCount;
					continue;
				}

				const std::optional<common::packet::PacketBuffer> responsePacket =
					BuildReliableDataPacket(
						peerPair.server,
						MakeConstPacketSpan(*responsePayload),
						currentTime
					);

				if (!responsePacket.has_value())
				{
					++buildFailureCount;
					continue;
				}

				const ReceiveResult responseReceiveResult =
					ReceiveReliablePacket(peerPair.client, *responsePacket);

				if (responseReceiveResult.parsed
					&& responseReceiveResult.isNewDataPacket
					&& responseReceiveResult.packetType == common::packet::PacketType::JoinRoomResponse)
				{
					++successfulResponseCount;
				}

				if (DeliverAckIfExists(peerPair.server, responseReceiveResult.ackPacketBuffer))
				{
					++ackDeliveryCount;
				}

				currentTime += std::chrono::milliseconds(1);
			}
		}

		const std::uint64_t expectedRoundTripCount =
			static_cast<std::uint64_t>(roundTripClientCount * roundTripRequestCountPerClient);

		tests::Expect(
			result,
			buildFailureCount == 0,
			"ReliableUdpLoad: many client round trip build failures"
		);
		tests::Expect(
			result,
			successfulRequestCount == expectedRoundTripCount,
			"ReliableUdpLoad: many client request delivery count"
		);
		tests::Expect(
			result,
			successfulResponseCount == expectedRoundTripCount,
			"ReliableUdpLoad: many client response delivery count"
		);
		tests::Expect(
			result,
			ackDeliveryCount == expectedRoundTripCount * 2,
			"ReliableUdpLoad: many client ack delivery count"
		);
		tests::Expect(
			result,
			GetTotalServerNewDataPacketCount(peerPairList) == expectedRoundTripCount,
			"ReliableUdpLoad: server received request count"
		);
		tests::Expect(
			result,
			GetTotalClientNewDataPacketCount(peerPairList) == expectedRoundTripCount,
			"ReliableUdpLoad: client received response count"
		);
		tests::Expect(
			result,
			HasNoPendingPackets(peerPairList),
			"ReliableUdpLoad: many client round trip pending packets cleared"
		);
	}

	void RunDroppedAckResendLoadTest(tests::DebugTestResult& result)
	{
		PeerPairList peerPairList = CreatePeerPairList(resendClientCount);

		TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		std::uint64_t successfulInitialRequestCount = 0;
		std::uint64_t extractedResendPacketCount = 0;
		std::uint64_t successfulAckAfterResendCount = 0;
		std::uint64_t buildFailureCount = 0;

		for (std::size_t requestIndex = 0; requestIndex < resendRequestCountPerClient; ++requestIndex)
		{
			for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
			{
				SimulatedPeerPair& peerPair = peerPairList[clientIndex];

				const std::int32_t roomId =
					static_cast<std::int32_t>((requestIndex + clientIndex) % 3) + 1;

				const std::optional<common::packet::PacketBuffer> requestPayload =
					SerializeJoinRoomRequest(roomId);

				if (!requestPayload.has_value())
				{
					++buildFailureCount;
					continue;
				}

				const std::optional<common::packet::PacketBuffer> requestPacket =
					BuildReliableDataPacket(
						peerPair.client,
						MakeConstPacketSpan(*requestPayload),
						currentTime
					);

				if (!requestPacket.has_value())
				{
					++buildFailureCount;
					continue;
				}

				const ReceiveResult firstReceiveResult =
					ReceiveReliablePacket(peerPair.server, *requestPacket);

				if (firstReceiveResult.parsed && firstReceiveResult.isNewDataPacket)
				{
					++successfulInitialRequestCount;
				}

				// Intentionally drop firstReceiveResult.ackPacketBuffer.
				currentTime += resendInterval;

				const common::net::ReliableUdpSession::ResendPacketList resendPacketList =
					peerPair.client.session.ExtractResendPackets(currentTime);

				extractedResendPacketCount += static_cast<std::uint64_t>(resendPacketList.size());

				for (const common::net::ReliablePendingPacket& resendPacket : resendPacketList)
				{
					const ReceiveResult resendReceiveResult =
						ReceiveReliablePacket(peerPair.server, resendPacket.packetBuffer);

					if (DeliverAckIfExists(peerPair.client, resendReceiveResult.ackPacketBuffer))
					{
						++successfulAckAfterResendCount;
					}
				}

				currentTime += std::chrono::milliseconds(1);
			}
		}

		const std::uint64_t expectedRequestCount =
			static_cast<std::uint64_t>(resendClientCount * resendRequestCountPerClient);

		tests::Expect(
			result,
			buildFailureCount == 0,
			"ReliableUdpLoad: dropped ack resend build failures"
		);
		tests::Expect(
			result,
			successfulInitialRequestCount == expectedRequestCount,
			"ReliableUdpLoad: dropped ack initial request count"
		);
		tests::Expect(
			result,
			extractedResendPacketCount == expectedRequestCount,
			"ReliableUdpLoad: dropped ack extracted resend count"
		);
		tests::Expect(
			result,
			successfulAckAfterResendCount == expectedRequestCount,
			"ReliableUdpLoad: dropped ack resend ack count"
		);
		tests::Expect(
			result,
			GetTotalServerDuplicateDataPacketCount(peerPairList) == expectedRequestCount,
			"ReliableUdpLoad: dropped ack duplicate request count"
		);
		tests::Expect(
			result,
			HasNoPendingPackets(peerPairList),
			"ReliableUdpLoad: dropped ack pending packets cleared"
		);
	}
}

namespace tests::net
{
	tests::DebugTestResult RunReliableUdpLoadTests()
	{
		tests::DebugTestResult result{};

		reliableUdpLoadTest::RunManyClientRoundTripLoadTest(result);
		reliableUdpLoadTest::RunDroppedAckResendLoadTest(result);

		return result;
	}
}