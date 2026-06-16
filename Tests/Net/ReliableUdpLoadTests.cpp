#include "ReliableUdpLoadTests.h"

#include <algorithm>
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

#include "ReliableUdpVirtualNetwork.h"

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

	static inline constexpr std::size_t roundTripClientCount = 32;
	static inline constexpr std::size_t roundTripRequestCountPerClient = 128;

	static inline constexpr std::size_t burstClientCount = 16;
	static inline constexpr std::size_t burstRequestCountPerClient = 64;

	static inline constexpr std::size_t giveUpClientCount = 8;
	static inline constexpr std::size_t giveUpRequestCountPerClient = 32;
	static inline constexpr int giveUpMaxResendCount = 3;

	static inline constexpr std::size_t resendClientCount = 16;
	static inline constexpr std::size_t resendRequestCountPerClient = 64;

	static inline constexpr std::chrono::milliseconds resendInterval = std::chrono::milliseconds(10);
	static inline constexpr std::size_t maxPendingPacketCount = 4096;
	static inline constexpr int maxResendCount = 8;

	static inline constexpr std::size_t virtualNetworkClientCount = 8;
	static inline constexpr std::size_t virtualNetworkRequestCountPerClient = 32;
	static inline constexpr int virtualNetworkMaxResendCount = 30;
	static inline constexpr int virtualNetworkMaxIterationCount = 2000;

	static inline constexpr std::size_t virtualNetworkRoundTripClientCount = 8;
	static inline constexpr std::size_t virtualNetworkRoundTripRequestCountPerClient = 24;
	static inline constexpr int virtualNetworkRoundTripMaxResendCount = 40;
	static inline constexpr int virtualNetworkRoundTripMaxIterationCount = 3000;

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

	[[nodiscard]] std::size_t GetMaxClientPendingPacketCount(
		const PeerPairList& peerPairList
	) noexcept
	{
		std::size_t maxPendingPacketCountValue = 0;

		for (const SimulatedPeerPair& peerPair : peerPairList)
		{
			maxPendingPacketCountValue = std::max(
				maxPendingPacketCountValue,
				peerPair.client.session.GetPendingPacketCount()
			);
		}

		return maxPendingPacketCountValue;
	}

	[[nodiscard]] bool HasNoVirtualNetworkPendingPackets(
		const std::vector<ReliableUdpVirtualNetwork>& virtualNetworkList
	) noexcept
	{
		for (const ReliableUdpVirtualNetwork& virtualNetwork : virtualNetworkList)
		{
			if (virtualNetwork.GetPendingPacketCount() != 0)
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

	void RunBurstOutOfOrderAckLoadTest(tests::DebugTestResult& result)
	{
		PeerPairList peerPairList = CreatePeerPairList(burstClientCount);

		std::vector<PacketBufferList> requestPacketTable(peerPairList.size());
		TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		std::uint64_t buildFailureCount = 0;
		std::uint64_t deliveredRequestCount = 0;
		std::uint64_t deliveredAckCount = 0;
		std::uint64_t expectedPendingPacketCount = 0;

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			SimulatedPeerPair& peerPair = peerPairList[clientIndex];
			PacketBufferList& requestPacketList = requestPacketTable[clientIndex];

			requestPacketList.reserve(burstRequestCountPerClient);

			for (std::size_t requestIndex = 0; requestIndex < burstRequestCountPerClient; ++requestIndex)
			{
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

				requestPacketList.push_back(*requestPacket);
				++expectedPendingPacketCount;

				currentTime += std::chrono::milliseconds(1);
			}
		}

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			SimulatedPeerPair& peerPair = peerPairList[clientIndex];

			tests::Expect(
				result,
				peerPair.client.session.GetPendingPacketCount() == burstRequestCountPerClient,
				"ReliableUdpLoad: burst pending count before ack"
			);
		}

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			SimulatedPeerPair& peerPair = peerPairList[clientIndex];
			const PacketBufferList& requestPacketList = requestPacketTable[clientIndex];

			PacketBufferList ackPacketList;
			ackPacketList.reserve(requestPacketList.size());

			for (const common::packet::PacketBuffer& requestPacket : requestPacketList)
			{
				const ReceiveResult receiveResult =
					ReceiveReliablePacket(peerPair.server, requestPacket);

				if (receiveResult.parsed
					&& receiveResult.isNewDataPacket
					&& receiveResult.packetType == common::packet::PacketType::JoinRoomRequest)
				{
					++deliveredRequestCount;
				}

				if (receiveResult.ackPacketBuffer.has_value())
				{
					ackPacketList.push_back(*receiveResult.ackPacketBuffer);
				}
			}

			for (std::size_t ackIndex = ackPacketList.size(); ackIndex > 0; --ackIndex)
			{
				if (DeliverAckIfExists(peerPair.client, ackPacketList[ackIndex - 1]))
				{
					++deliveredAckCount;
				}
			}
		}

		const std::uint64_t expectedRequestCount =
			static_cast<std::uint64_t>(burstClientCount * burstRequestCountPerClient);

		tests::Expect(
			result,
			buildFailureCount == 0,
			"ReliableUdpLoad: burst out-of-order ack build failures"
		);
		tests::Expect(
			result,
			expectedPendingPacketCount == expectedRequestCount,
			"ReliableUdpLoad: burst expected pending packet count"
		);
		tests::Expect(
			result,
			deliveredRequestCount == expectedRequestCount,
			"ReliableUdpLoad: burst request delivery count"
		);
		tests::Expect(
			result,
			deliveredAckCount == expectedRequestCount,
			"ReliableUdpLoad: burst out-of-order ack delivery count"
		);
		tests::Expect(
			result,
			GetTotalServerNewDataPacketCount(peerPairList) == expectedRequestCount,
			"ReliableUdpLoad: burst server received request count"
		);
		tests::Expect(
			result,
			HasNoPendingPackets(peerPairList),
			"ReliableUdpLoad: burst pending packets cleared"
		);
	}

	void RunGiveUpLoadTest(tests::DebugTestResult& result)
	{
		PeerPairList peerPairList = CreatePeerPairList(giveUpClientCount);

		TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		std::uint64_t buildFailureCount = 0;
		std::uint64_t registeredRequestCount = 0;
		std::uint64_t extractedResendPacketCount = 0;
		std::uint64_t giveUpPacketCount = 0;

		for (SimulatedPeerPair& peerPair : peerPairList)
		{
			peerPair.client.session.SetMaxResendCount(giveUpMaxResendCount);
		}

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			SimulatedPeerPair& peerPair = peerPairList[clientIndex];

			for (std::size_t requestIndex = 0; requestIndex < giveUpRequestCountPerClient; ++requestIndex)
			{
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

				// Intentionally do not deliver the packet and do not deliver ACK.
				++registeredRequestCount;
				currentTime += std::chrono::milliseconds(1);
			}
		}

		const std::uint64_t expectedRequestCount =
			static_cast<std::uint64_t>(giveUpClientCount * giveUpRequestCountPerClient);

		tests::Expect(
			result,
			GetMaxClientPendingPacketCount(peerPairList) == giveUpRequestCountPerClient,
			"ReliableUdpLoad: give-up pending count before resend"
		);

		for (int resendAttempt = 0; resendAttempt <= giveUpMaxResendCount; ++resendAttempt)
		{
			currentTime += resendInterval;

			for (SimulatedPeerPair& peerPair : peerPairList)
			{
				const common::net::ReliableUdpSession::ResendResult resendResult =
					peerPair.client.session.ExtractResendResult(currentTime);

				extractedResendPacketCount +=
					static_cast<std::uint64_t>(resendResult.resendPacketList.size());

				giveUpPacketCount +=
					static_cast<std::uint64_t>(resendResult.giveUpPacketList.size());
			}
		}

		tests::Expect(
			result,
			buildFailureCount == 0,
			"ReliableUdpLoad: give-up build failures"
		);
		tests::Expect(
			result,
			registeredRequestCount == expectedRequestCount,
			"ReliableUdpLoad: give-up registered request count"
		);
		tests::Expect(
			result,
			extractedResendPacketCount == expectedRequestCount * giveUpMaxResendCount,
			"ReliableUdpLoad: give-up resend extraction count"
		);
		tests::Expect(
			result,
			giveUpPacketCount == expectedRequestCount,
			"ReliableUdpLoad: give-up packet count"
		);
		tests::Expect(
			result,
			HasNoPendingPackets(peerPairList),
			"ReliableUdpLoad: give-up pending packets cleared"
		);
	}

	void RunVirtualNetworkFaultLoadTest(tests::DebugTestResult& result)
	{
		PeerPairList peerPairList = CreatePeerPairList(virtualNetworkClientCount);
		std::vector<ReliableUdpVirtualNetwork> virtualNetworkList(peerPairList.size());

		ReliableUdpVirtualNetwork::Config virtualNetworkConfig{};
		virtualNetworkConfig.dropModulo = 5;
		virtualNetworkConfig.duplicateModulo = 7;
		virtualNetworkConfig.delayModulo = 3;
		virtualNetworkConfig.reorderModulo = 4;
		virtualNetworkConfig.delay = std::chrono::milliseconds(10);
		virtualNetworkConfig.reorderDelay = std::chrono::milliseconds(30);

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			peerPairList[clientIndex].client.session.SetMaxResendCount(virtualNetworkMaxResendCount);
			peerPairList[clientIndex].client.session.SetResendInterval(resendInterval);

			virtualNetworkList[clientIndex].SetConfig(virtualNetworkConfig);
		}

		TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		std::uint64_t buildFailureCount = 0;
		std::uint64_t submittedRequestCount = 0;
		std::uint64_t deliveredRequestCount = 0;
		std::uint64_t deliveredAckOnlyPacketCount = 0;
		std::uint64_t extractedResendPacketCount = 0;
		std::uint64_t giveUpPacketCount = 0;

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			SimulatedPeerPair& peerPair = peerPairList[clientIndex];
			ReliableUdpVirtualNetwork& virtualNetwork = virtualNetworkList[clientIndex];

			for (std::size_t requestIndex = 0; requestIndex < virtualNetworkRequestCountPerClient; ++requestIndex)
			{
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

				virtualNetwork.Submit(
					ReliableUdpVirtualNetwork::Endpoint::Client,
					*requestPacket,
					currentTime
				);

				++submittedRequestCount;
				currentTime += std::chrono::milliseconds(1);
			}
		}

		const std::uint64_t expectedRequestCount =
			static_cast<std::uint64_t>(
				virtualNetworkClientCount * virtualNetworkRequestCountPerClient
				);

		bool completed = false;

		for (int iteration = 0; iteration < virtualNetworkMaxIterationCount; ++iteration)
		{
			currentTime += std::chrono::milliseconds(5);

			for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
			{
				SimulatedPeerPair& peerPair = peerPairList[clientIndex];
				ReliableUdpVirtualNetwork& virtualNetwork = virtualNetworkList[clientIndex];

				const common::net::ReliableUdpSession::ResendResult resendResult =
					peerPair.client.session.ExtractResendResult(currentTime);

				extractedResendPacketCount +=
					static_cast<std::uint64_t>(resendResult.resendPacketList.size());

				giveUpPacketCount +=
					static_cast<std::uint64_t>(resendResult.giveUpPacketList.size());

				for (const common::net::ReliablePendingPacket& resendPacket :
					resendResult.resendPacketList)
				{
					virtualNetwork.Submit(
						ReliableUdpVirtualNetwork::Endpoint::Client,
						resendPacket.packetBuffer,
						currentTime
					);
				}

				ReliableUdpVirtualNetwork::PacketList serverPacketList =
					virtualNetwork.ExtractReadyPackets(
						ReliableUdpVirtualNetwork::Endpoint::Server,
						currentTime
					);

				for (const ReliableUdpVirtualNetwork::Packet& serverPacket :
					serverPacketList)
				{
					const ReceiveResult receiveResult =
						ReceiveReliablePacket(
							peerPair.server,
							serverPacket.packetBuffer
						);

					if (receiveResult.parsed
						&& receiveResult.isNewDataPacket
						&& receiveResult.packetType == common::packet::PacketType::JoinRoomRequest)
					{
						++deliveredRequestCount;
					}

					if (receiveResult.ackPacketBuffer.has_value())
					{
						virtualNetwork.Submit(
							ReliableUdpVirtualNetwork::Endpoint::Server,
							*receiveResult.ackPacketBuffer,
							currentTime
						);
					}
				}

				ReliableUdpVirtualNetwork::PacketList clientPacketList =
					virtualNetwork.ExtractReadyPackets(
						ReliableUdpVirtualNetwork::Endpoint::Client,
						currentTime
					);

				for (const ReliableUdpVirtualNetwork::Packet& clientPacket :
					clientPacketList)
				{
					const ReceiveResult receiveResult =
						ReceiveReliablePacket(
							peerPair.client,
							clientPacket.packetBuffer
						);

					if (receiveResult.parsed && receiveResult.isAckOnly)
					{
						++deliveredAckOnlyPacketCount;
					}
				}
			}

			if (deliveredRequestCount == expectedRequestCount
				&& HasNoPendingPackets(peerPairList)
				&& HasNoVirtualNetworkPendingPackets(virtualNetworkList))
			{
				completed = true;
				break;
			}
		}

		tests::Expect(
			result,
			buildFailureCount == 0,
			"ReliableUdpLoad: virtual network build failures"
		);
		tests::Expect(
			result,
			submittedRequestCount == expectedRequestCount,
			"ReliableUdpLoad: virtual network submitted request count"
		);
		tests::Expect(
			result,
			deliveredRequestCount == expectedRequestCount,
			"ReliableUdpLoad: virtual network delivered request count"
		);
		tests::Expect(
			result,
			deliveredAckOnlyPacketCount >= expectedRequestCount,
			"ReliableUdpLoad: virtual network ack delivery count"
		);
		tests::Expect(
			result,
			extractedResendPacketCount > 0,
			"ReliableUdpLoad: virtual network resend occurred"
		);
		tests::Expect(
			result,
			giveUpPacketCount == 0,
			"ReliableUdpLoad: virtual network no give-up"
		);
		tests::Expect(
			result,
			completed,
			"ReliableUdpLoad: virtual network completed"
		);
		tests::Expect(
			result,
			HasNoPendingPackets(peerPairList),
			"ReliableUdpLoad: virtual network reliable pending cleared"
		);
		tests::Expect(
			result,
			HasNoVirtualNetworkPendingPackets(virtualNetworkList),
			"ReliableUdpLoad: virtual network pending cleared"
		);
	}

	void RunVirtualNetworkRoundTripFaultLoadTest(tests::DebugTestResult& result)
	{
		PeerPairList peerPairList = CreatePeerPairList(virtualNetworkRoundTripClientCount);
		std::vector<ReliableUdpVirtualNetwork> virtualNetworkList(peerPairList.size());

		ReliableUdpVirtualNetwork::Config virtualNetworkConfig{};
		virtualNetworkConfig.dropModulo = 5;
		virtualNetworkConfig.duplicateModulo = 7;
		virtualNetworkConfig.delayModulo = 3;
		virtualNetworkConfig.reorderModulo = 4;
		virtualNetworkConfig.delay = std::chrono::milliseconds(10);
		virtualNetworkConfig.reorderDelay = std::chrono::milliseconds(30);

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			peerPairList[clientIndex].client.session.SetMaxResendCount(
				virtualNetworkRoundTripMaxResendCount
			);
			peerPairList[clientIndex].server.session.SetMaxResendCount(
				virtualNetworkRoundTripMaxResendCount
			);

			peerPairList[clientIndex].client.session.SetResendInterval(resendInterval);
			peerPairList[clientIndex].server.session.SetResendInterval(resendInterval);

			virtualNetworkList[clientIndex].SetConfig(virtualNetworkConfig);
		}

		TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		std::uint64_t buildFailureCount = 0;
		std::uint64_t submittedRequestCount = 0;
		std::uint64_t submittedResponseCount = 0;
		std::uint64_t deliveredRequestCount = 0;
		std::uint64_t deliveredResponseCount = 0;
		std::uint64_t deliveredClientAckOnlyPacketCount = 0;
		std::uint64_t deliveredServerAckOnlyPacketCount = 0;
		std::uint64_t extractedClientResendPacketCount = 0;
		std::uint64_t extractedServerResendPacketCount = 0;
		std::uint64_t clientGiveUpPacketCount = 0;
		std::uint64_t serverGiveUpPacketCount = 0;

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			SimulatedPeerPair& peerPair = peerPairList[clientIndex];
			ReliableUdpVirtualNetwork& virtualNetwork = virtualNetworkList[clientIndex];

			for (std::size_t requestIndex = 0; requestIndex < virtualNetworkRoundTripRequestCountPerClient; ++requestIndex)
			{
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

				virtualNetwork.Submit(
					ReliableUdpVirtualNetwork::Endpoint::Client,
					*requestPacket,
					currentTime
				);

				++submittedRequestCount;
				currentTime += std::chrono::milliseconds(1);
			}
		}

		const std::uint64_t expectedRoundTripCount =
			static_cast<std::uint64_t>(
				virtualNetworkRoundTripClientCount * virtualNetworkRoundTripRequestCountPerClient
				);

		bool completed = false;

		for (int iteration = 0; iteration < virtualNetworkRoundTripMaxIterationCount; ++iteration)
		{
			currentTime += std::chrono::milliseconds(5);

			for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
			{
				SimulatedPeerPair& peerPair = peerPairList[clientIndex];
				ReliableUdpVirtualNetwork& virtualNetwork = virtualNetworkList[clientIndex];

				const common::net::ReliableUdpSession::ResendResult clientResendResult =
					peerPair.client.session.ExtractResendResult(currentTime);

				extractedClientResendPacketCount +=
					static_cast<std::uint64_t>(clientResendResult.resendPacketList.size());

				clientGiveUpPacketCount +=
					static_cast<std::uint64_t>(clientResendResult.giveUpPacketList.size());

				for (const common::net::ReliablePendingPacket& resendPacket :
					clientResendResult.resendPacketList)
				{
					virtualNetwork.Submit(
						ReliableUdpVirtualNetwork::Endpoint::Client,
						resendPacket.packetBuffer,
						currentTime
					);
				}

				const common::net::ReliableUdpSession::ResendResult serverResendResult =
					peerPair.server.session.ExtractResendResult(currentTime);

				extractedServerResendPacketCount +=
					static_cast<std::uint64_t>(serverResendResult.resendPacketList.size());

				serverGiveUpPacketCount +=
					static_cast<std::uint64_t>(serverResendResult.giveUpPacketList.size());

				for (const common::net::ReliablePendingPacket& resendPacket :
					serverResendResult.resendPacketList)
				{
					virtualNetwork.Submit(
						ReliableUdpVirtualNetwork::Endpoint::Server,
						resendPacket.packetBuffer,
						currentTime
					);
				}

				ReliableUdpVirtualNetwork::PacketList serverPacketList =
					virtualNetwork.ExtractReadyPackets(
						ReliableUdpVirtualNetwork::Endpoint::Server,
						currentTime
					);

				for (const ReliableUdpVirtualNetwork::Packet& serverPacket :
					serverPacketList)
				{
					const ReceiveResult receiveResult =
						ReceiveReliablePacket(
							peerPair.server,
							serverPacket.packetBuffer
						);

					if (receiveResult.parsed && receiveResult.isAckOnly)
					{
						++deliveredServerAckOnlyPacketCount;
						continue;
					}

					if (receiveResult.ackPacketBuffer.has_value())
					{
						virtualNetwork.Submit(
							ReliableUdpVirtualNetwork::Endpoint::Server,
							*receiveResult.ackPacketBuffer,
							currentTime
						);
					}

					if (!receiveResult.parsed
						|| !receiveResult.isNewDataPacket
						|| receiveResult.packetType != common::packet::PacketType::JoinRoomRequest)
					{
						continue;
					}

					++deliveredRequestCount;

					const std::int32_t roomId =
						static_cast<std::int32_t>((clientIndex + deliveredRequestCount) % 3) + 1;

					const std::optional<common::packet::PacketBuffer> responsePayload =
						SerializeJoinRoomResponse(
							roomId,
							100.0F + static_cast<float>(clientIndex),
							200.0F + static_cast<float>(deliveredRequestCount)
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

					virtualNetwork.Submit(
						ReliableUdpVirtualNetwork::Endpoint::Server,
						*responsePacket,
						currentTime
					);

					++submittedResponseCount;
				}

				ReliableUdpVirtualNetwork::PacketList clientPacketList =
					virtualNetwork.ExtractReadyPackets(
						ReliableUdpVirtualNetwork::Endpoint::Client,
						currentTime
					);

				for (const ReliableUdpVirtualNetwork::Packet& clientPacket :
					clientPacketList)
				{
					const ReceiveResult receiveResult =
						ReceiveReliablePacket(
							peerPair.client,
							clientPacket.packetBuffer
						);

					if (receiveResult.parsed && receiveResult.isAckOnly)
					{
						++deliveredClientAckOnlyPacketCount;
						continue;
					}

					if (receiveResult.ackPacketBuffer.has_value())
					{
						virtualNetwork.Submit(
							ReliableUdpVirtualNetwork::Endpoint::Client,
							*receiveResult.ackPacketBuffer,
							currentTime
						);
					}

					if (receiveResult.parsed
						&& receiveResult.isNewDataPacket
						&& receiveResult.packetType == common::packet::PacketType::JoinRoomResponse)
					{
						++deliveredResponseCount;
					}
				}
			}

			if (deliveredRequestCount == expectedRoundTripCount
				&& submittedResponseCount == expectedRoundTripCount
				&& deliveredResponseCount == expectedRoundTripCount
				&& HasNoPendingPackets(peerPairList)
				&& HasNoVirtualNetworkPendingPackets(virtualNetworkList))
			{
				completed = true;
				break;
			}
		}

		tests::Expect(
			result,
			buildFailureCount == 0,
			"ReliableUdpLoad: virtual network round trip build failures"
		);
		tests::Expect(
			result,
			submittedRequestCount == expectedRoundTripCount,
			"ReliableUdpLoad: virtual network round trip submitted request count"
		);
		tests::Expect(
			result,
			deliveredRequestCount == expectedRoundTripCount,
			"ReliableUdpLoad: virtual network round trip delivered request count"
		);
		tests::Expect(
			result,
			submittedResponseCount == expectedRoundTripCount,
			"ReliableUdpLoad: virtual network round trip submitted response count"
		);
		tests::Expect(
			result,
			deliveredResponseCount == expectedRoundTripCount,
			"ReliableUdpLoad: virtual network round trip delivered response count"
		);
		tests::Expect(
			result,
			deliveredClientAckOnlyPacketCount >= expectedRoundTripCount,
			"ReliableUdpLoad: virtual network round trip client ack delivery count"
		);
		tests::Expect(
			result,
			deliveredServerAckOnlyPacketCount >= expectedRoundTripCount,
			"ReliableUdpLoad: virtual network round trip server ack delivery count"
		);
		tests::Expect(
			result,
			extractedClientResendPacketCount > 0,
			"ReliableUdpLoad: virtual network round trip client resend occurred"
		);
		tests::Expect(
			result,
			extractedServerResendPacketCount > 0,
			"ReliableUdpLoad: virtual network round trip server resend occurred"
		);
		tests::Expect(
			result,
			clientGiveUpPacketCount == 0,
			"ReliableUdpLoad: virtual network round trip client no give-up"
		);
		tests::Expect(
			result,
			serverGiveUpPacketCount == 0,
			"ReliableUdpLoad: virtual network round trip server no give-up"
		);
		tests::Expect(
			result,
			completed,
			"ReliableUdpLoad: virtual network round trip completed"
		);
		tests::Expect(
			result,
			HasNoPendingPackets(peerPairList),
			"ReliableUdpLoad: virtual network round trip reliable pending cleared"
		);
		tests::Expect(
			result,
			HasNoVirtualNetworkPendingPackets(virtualNetworkList),
			"ReliableUdpLoad: virtual network round trip network pending cleared"
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
		reliableUdpLoadTest::RunBurstOutOfOrderAckLoadTest(result);
		reliableUdpLoadTest::RunGiveUpLoadTest(result);
		reliableUdpLoadTest::RunVirtualNetworkFaultLoadTest(result);
		reliableUdpLoadTest::RunVirtualNetworkRoundTripFaultLoadTest(result);

		return result;
	}
}