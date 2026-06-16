#include "ReliableUdpLoadTestHarness.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <Common/Packet/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Packet/ReliableUdpPacketBuilder.h>

namespace tests::net::reliableUdpLoadTest
{
	common::packet::ConstPacketSpan MakeConstPacketSpan(
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

	PeerPairList CreatePeerPairList(std::size_t peerPairCount)
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

	ReliableUdpVirtualNetwork::Config MakeDefaultVirtualNetworkFaultConfig() noexcept
	{
		ReliableUdpVirtualNetwork::Config config{};
		config.dropModulo = 5;
		config.duplicateModulo = 7;
		config.delayModulo = 3;
		config.reorderModulo = 4;
		config.delay = std::chrono::milliseconds(10);
		config.reorderDelay = std::chrono::milliseconds(30);

		return config;
	}

	void ConfigurePeerForVirtualNetwork(
		SimulatedPeer& peer,
		int maxResendCountValue
	) noexcept
	{
		peer.session.SetMaxResendCount(maxResendCountValue);
		peer.session.SetResendInterval(resendInterval);
	}

	VirtualNetworkResendPumpResult SubmitResendPacketsToVirtualNetwork(
		SimulatedPeer& sender,
		ReliableUdpVirtualNetwork& virtualNetwork,
		ReliableUdpVirtualNetwork::Endpoint sourceEndpoint,
		TimePoint currentTime
	)
	{
		const common::net::ReliableUdpSession::ResendResult resendResult =
			sender.session.ExtractResendResult(currentTime);

		VirtualNetworkResendPumpResult result{};
		result.extractedResendPacketCount =
			static_cast<std::uint64_t>(
				resendResult.resendPacketList.size()
				);
		result.giveUpPacketCount =
			static_cast<std::uint64_t>(
				resendResult.giveUpPacketList.size()
				);

		for (const common::net::ReliablePendingPacket& resendPacket :
			resendResult.resendPacketList)
		{
			virtualNetwork.Submit(
				sourceEndpoint,
				resendPacket.packetBuffer,
				currentTime
			);
		}

		return result;
	}

	VirtualNetworkReceivePumpResult PumpReadyPacketsToReceiver(
		SimulatedPeer& receiver,
		ReliableUdpVirtualNetwork& virtualNetwork,
		ReliableUdpVirtualNetwork::Endpoint receiverEndpoint,
		common::packet::PacketType targetPacketType,
		TimePoint currentTime
	)
	{
		VirtualNetworkReceivePumpResult result{};

		ReliableUdpVirtualNetwork::PacketList packetList =
			virtualNetwork.ExtractReadyPackets(
				receiverEndpoint,
				currentTime
			);

		for (const ReliableUdpVirtualNetwork::Packet& packet : packetList)
		{
			const ReceiveResult receiveResult =
				ReceiveReliablePacket(
					receiver,
					packet.packetBuffer
				);

			if (receiveResult.parsed && receiveResult.isAckOnly)
			{
				++result.deliveredAckOnlyPacketCount;
				continue;
			}

			if (receiveResult.ackPacketBuffer.has_value())
			{
				virtualNetwork.Submit(
					receiverEndpoint,
					*receiveResult.ackPacketBuffer,
					currentTime
				);

				++result.submittedAckPacketCount;
			}

			if (receiveResult.parsed && receiveResult.isNewDataPacket)
			{
				++result.deliveredNewDataPacketCount;
			}

			if (receiveResult.parsed
				&& receiveResult.isNewDataPacket
				&& receiveResult.packetType == targetPacketType)
			{
				++result.deliveredTargetPacketCount;
			}
		}

		return result;
	}

	VirtualNetworkRequestResponsePumpResult PumpJoinRoomRequestsAndSubmitResponses(
		SimulatedPeer& serverPeer,
		ReliableUdpVirtualNetwork& virtualNetwork,
		std::size_t clientIndex,
		TimePoint currentTime
	)
	{
		VirtualNetworkRequestResponsePumpResult result{};

		ReliableUdpVirtualNetwork::PacketList packetList =
			virtualNetwork.ExtractReadyPackets(
				ReliableUdpVirtualNetwork::Endpoint::Server,
				currentTime
			);

		for (const ReliableUdpVirtualNetwork::Packet& packet : packetList)
		{
			const ReceiveResult receiveResult =
				ReceiveReliablePacket(
					serverPeer,
					packet.packetBuffer
				);

			if (receiveResult.parsed && receiveResult.isAckOnly)
			{
				++result.deliveredAckOnlyPacketCount;
				continue;
			}

			if (receiveResult.ackPacketBuffer.has_value())
			{
				virtualNetwork.Submit(
					ReliableUdpVirtualNetwork::Endpoint::Server,
					*receiveResult.ackPacketBuffer,
					currentTime
				);

				++result.submittedAckPacketCount;
			}

			if (!receiveResult.parsed
				|| !receiveResult.isNewDataPacket
				|| receiveResult.packetType != common::packet::PacketType::JoinRoomRequest)
			{
				continue;
			}

			++result.deliveredRequestPacketCount;

			const std::int32_t roomId =
				static_cast<std::int32_t>(
					(clientIndex + result.deliveredRequestPacketCount) % 3
					) + 1;

			const std::optional<common::packet::PacketBuffer> responsePayload =
				SerializeJoinRoomResponse(
					roomId,
					100.0F + static_cast<float>(clientIndex),
					200.0F + static_cast<float>(result.deliveredRequestPacketCount)
				);

			if (!responsePayload.has_value())
			{
				++result.buildFailureCount;
				continue;
			}

			const std::optional<common::packet::PacketBuffer> responsePacket =
				BuildReliableDataPacket(
					serverPeer,
					MakeConstPacketSpan(*responsePayload),
					currentTime
				);

			if (!responsePacket.has_value())
			{
				++result.buildFailureCount;
				continue;
			}

			virtualNetwork.Submit(
				ReliableUdpVirtualNetwork::Endpoint::Server,
				*responsePacket,
				currentTime
			);

			++result.submittedResponsePacketCount;
		}

		return result;
	}

	VirtualNetworkRequestSubmitResult SubmitJoinRoomRequestsToVirtualNetworks(
		PeerPairList& peerPairList,
		std::vector<ReliableUdpVirtualNetwork>& virtualNetworkList,
		std::size_t requestCountPerClient,
		TimePoint& currentTime
	)
	{
		VirtualNetworkRequestSubmitResult result{};

		for (std::size_t clientIndex = 0; clientIndex < peerPairList.size(); ++clientIndex)
		{
			SimulatedPeerPair& peerPair = peerPairList[clientIndex];
			ReliableUdpVirtualNetwork& virtualNetwork = virtualNetworkList[clientIndex];

			for (std::size_t requestIndex = 0; requestIndex < requestCountPerClient; ++requestIndex)
			{
				const std::int32_t roomId =
					static_cast<std::int32_t>((requestIndex + clientIndex) % 3) + 1;

				const std::optional<common::packet::PacketBuffer> requestPayload =
					SerializeJoinRoomRequest(roomId);

				if (!requestPayload.has_value())
				{
					++result.buildFailureCount;
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
					++result.buildFailureCount;
					continue;
				}

				virtualNetwork.Submit(
					ReliableUdpVirtualNetwork::Endpoint::Client,
					*requestPacket,
					currentTime
				);

				++result.submittedRequestCount;
				currentTime += std::chrono::milliseconds(1);
			}
		}

		return result;
	}

	std::optional<common::packet::PacketBuffer> SerializeJoinRoomRequest(
		std::int32_t roomId
	)
	{
		common::packet::JoinRoomRequestPacket packet{};
		packet.roomId = roomId;

		return common::packet::SerializePacket(packet);
	}

	std::optional<common::packet::PacketBuffer> SerializeJoinRoomResponse(
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

	std::optional<common::packet::PacketBuffer> BuildReliableDataPacket(
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

	std::optional<common::packet::PacketBuffer> BuildReliableAckPacket(
		SimulatedPeer& sender
	)
	{
		const common::net::ReliableUdpPacketHeader reliableHeader =
			sender.session.BuildOutgoingAckHeader();

		return common::packet::BuildReliableUdpAckPacket(reliableHeader);
	}

	ReceiveResult ReceiveReliablePacket(
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

	bool DeliverAckIfExists(
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

	bool HasNoPendingPackets(
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

	bool HasNoVirtualNetworkPendingPackets(
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

	std::size_t GetMaxClientPendingPacketCount(
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

	std::uint64_t GetTotalClientNewDataPacketCount(
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

	std::uint64_t GetTotalServerNewDataPacketCount(
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

	std::uint64_t GetTotalServerDuplicateDataPacketCount(
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
}