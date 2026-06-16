#include "ReliableUdpLoadTests.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <Common/Net/ReliableUdpSession.h>
#include <Common/Packet/PacketType.h>

#include <Tests/DebugTestResult.h>

#include "ReliableUdpLoadTestHarness.h"
#include "ReliableUdpVirtualNetwork.h"

namespace tests::net::reliableUdpLoadTest
{
	static inline constexpr std::size_t roundTripClientCount = 32;
	static inline constexpr std::size_t roundTripRequestCountPerClient = 128;

	static inline constexpr std::size_t burstClientCount = 16;
	static inline constexpr std::size_t burstRequestCountPerClient = 64;

	static inline constexpr std::size_t giveUpClientCount = 8;
	static inline constexpr std::size_t giveUpRequestCountPerClient = 32;
	static inline constexpr int giveUpMaxResendCount = 3;

	static inline constexpr std::size_t resendClientCount = 16;
	static inline constexpr std::size_t resendRequestCountPerClient = 64;

	static inline constexpr std::size_t virtualNetworkClientCount = 8;
	static inline constexpr std::size_t virtualNetworkRequestCountPerClient = 32;
	static inline constexpr int virtualNetworkMaxResendCount = 30;
	static inline constexpr int virtualNetworkMaxIterationCount = 2000;

	static inline constexpr std::size_t virtualNetworkRoundTripClientCount = 8;
	static inline constexpr std::size_t virtualNetworkRoundTripRequestCountPerClient = 24;
	static inline constexpr int virtualNetworkRoundTripMaxResendCount = 40;
	static inline constexpr int virtualNetworkRoundTripMaxIterationCount = 3000;

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

		const ReliableUdpVirtualNetwork::Config virtualNetworkConfig = MakeDefaultVirtualNetworkFaultConfig();

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			ConfigurePeerForVirtualNetwork(
				peerPairList[clientIndex].client,
				virtualNetworkMaxResendCount
			);

			virtualNetworkList[clientIndex].SetConfig(virtualNetworkConfig);
		}

		TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		std::uint64_t buildFailureCount = 0;
		std::uint64_t submittedRequestCount = 0;
		std::uint64_t deliveredRequestCount = 0;
		std::uint64_t deliveredAckOnlyPacketCount = 0;
		std::uint64_t extractedResendPacketCount = 0;
		std::uint64_t giveUpPacketCount = 0;

		const VirtualNetworkRequestSubmitResult requestSubmitResult =
			SubmitJoinRoomRequestsToVirtualNetworks(
				peerPairList,
				virtualNetworkList,
				virtualNetworkRequestCountPerClient,
				currentTime
			);

		buildFailureCount += requestSubmitResult.buildFailureCount;
		submittedRequestCount += requestSubmitResult.submittedRequestCount;

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

				const VirtualNetworkResendPumpResult resendPumpResult =
					SubmitResendPacketsToVirtualNetwork(
						peerPair.client,
						virtualNetwork,
						ReliableUdpVirtualNetwork::Endpoint::Client,
						currentTime
					);

				extractedResendPacketCount += resendPumpResult.extractedResendPacketCount;
				giveUpPacketCount += resendPumpResult.giveUpPacketCount;

				const VirtualNetworkReceivePumpResult serverReceivePumpResult =
					PumpReadyPacketsToReceiver(
						peerPair.server,
						virtualNetwork,
						ReliableUdpVirtualNetwork::Endpoint::Server,
						common::packet::PacketType::JoinRoomRequest,
						currentTime
					);

				deliveredRequestCount +=
					serverReceivePumpResult.deliveredTargetPacketCount;

				const VirtualNetworkReceivePumpResult clientReceivePumpResult =
					PumpReadyPacketsToReceiver(
						peerPair.client,
						virtualNetwork,
						ReliableUdpVirtualNetwork::Endpoint::Client,
						common::packet::PacketType::None,
						currentTime
					);

				deliveredAckOnlyPacketCount +=
					clientReceivePumpResult.deliveredAckOnlyPacketCount;
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

		const ReliableUdpVirtualNetwork::Config virtualNetworkConfig = MakeDefaultVirtualNetworkFaultConfig();

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			ConfigurePeerForVirtualNetwork(
				peerPairList[clientIndex].client,
				virtualNetworkRoundTripMaxResendCount
			);
			ConfigurePeerForVirtualNetwork(
				peerPairList[clientIndex].server,
				virtualNetworkRoundTripMaxResendCount
			);

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

		const VirtualNetworkRequestSubmitResult requestSubmitResult =
			SubmitJoinRoomRequestsToVirtualNetworks(
				peerPairList,
				virtualNetworkList,
				virtualNetworkRoundTripRequestCountPerClient,
				currentTime
			);

		buildFailureCount += requestSubmitResult.buildFailureCount;
		submittedRequestCount += requestSubmitResult.submittedRequestCount;

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

				const VirtualNetworkResendPumpResult clientResendPumpResult =
					SubmitResendPacketsToVirtualNetwork(
						peerPair.client,
						virtualNetwork,
						ReliableUdpVirtualNetwork::Endpoint::Client,
						currentTime
					);

				extractedClientResendPacketCount +=
					clientResendPumpResult.extractedResendPacketCount;

				clientGiveUpPacketCount +=
					clientResendPumpResult.giveUpPacketCount;

				const VirtualNetworkResendPumpResult serverResendPumpResult =
					SubmitResendPacketsToVirtualNetwork(
						peerPair.server,
						virtualNetwork,
						ReliableUdpVirtualNetwork::Endpoint::Server,
						currentTime
					);

				extractedServerResendPacketCount +=
					serverResendPumpResult.extractedResendPacketCount;

				serverGiveUpPacketCount +=
					serverResendPumpResult.giveUpPacketCount;

				const VirtualNetworkRequestResponsePumpResult requestResponsePumpResult =
					PumpJoinRoomRequestsAndSubmitResponses(
						peerPair.server,
						virtualNetwork,
						clientIndex,
						currentTime
					);

				deliveredServerAckOnlyPacketCount +=
					requestResponsePumpResult.deliveredAckOnlyPacketCount;

				deliveredRequestCount +=
					requestResponsePumpResult.deliveredRequestPacketCount;

				submittedResponseCount +=
					requestResponsePumpResult.submittedResponsePacketCount;

				buildFailureCount +=
					requestResponsePumpResult.buildFailureCount;

				const VirtualNetworkReceivePumpResult clientReceivePumpResult =
					PumpReadyPacketsToReceiver(
						peerPair.client,
						virtualNetwork,
						ReliableUdpVirtualNetwork::Endpoint::Client,
						common::packet::PacketType::JoinRoomResponse,
						currentTime
					);

				deliveredClientAckOnlyPacketCount +=
					clientReceivePumpResult.deliveredAckOnlyPacketCount;

				deliveredResponseCount +=
					clientReceivePumpResult.deliveredTargetPacketCount;
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