#include "UdpPacketSender.h"

#include <optional>

#include <Common/Net/Endpoint.h>
#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>

#include <Server/Net/UdpIocpTransport.h>

namespace
{
	template <typename TAddress, typename TPacket>
	[[nodiscard]] bool SendSerializedPacket(server::net::UdpPacketSender& packetSender, const TAddress& address, const TPacket& packet)
	{
		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return false;
		}

		return packetSender.SendPacket(address, packetBuffer->data(), static_cast<int>(packetBuffer->size()));
	}

	template <typename TPacket>
	[[nodiscard]] std::size_t BroadcastSerializedPacket(
		server::net::UdpPacketSender& packetSender,
		std::span<const common::net::EndpointKey> endpointKeyList,
		const TPacket& packet
	)
	{
		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return 0;
		}

		return packetSender.BroadcastPacket(endpointKeyList, packetBuffer->data(), static_cast<int>(packetBuffer->size()));
	}
}

namespace server::net
{
	void UdpPacketSender::AttachTransport(UdpIocpTransport& udpTransport) noexcept
	{
		udpTransport_ = &udpTransport;
	}

	void UdpPacketSender::DetachTransport() noexcept
	{
		udpTransport_ = nullptr;
	}

	bool UdpPacketSender::SendPacket(const common::net::EndpointKey& endpointKey, const void* packetData, int packetSize)
	{
		return SendPacket(common::net::MakeSocketAddress(endpointKey), packetData, packetSize);
	}

	bool UdpPacketSender::SendPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize)
	{
		if (udpTransport_ == nullptr || packetData == nullptr || packetSize <= 0)
		{
			return false;
		}

		if (!faultSimulator_.IsEnabled())
		{
			return SendRawPacket(remoteAddress, packetData, packetSize);
		}

		const auto* packetBytes = static_cast<const char*>(packetData);

		common::net::UdpFaultSimulator::SubmitResult submitResult =
			faultSimulator_.Submit(
				remoteAddress,
				common::packet::ConstPacketSpan(
					packetBytes,
					static_cast<std::size_t>(packetSize)
				),
				common::time::Clock::now()
			);

		if (submitResult.dropped)
		{
			return true;
		}

		bool allSent = true;

		for (const common::net::UdpFaultSimulator::Packet& packet : submitResult.readyPacketList)
		{
			if (!SendRawPacket(
				packet.remoteAddress,
				packet.packetBuffer.data(),
				static_cast<int>(packet.packetBuffer.size())
			))
			{
				allSent = false;
			}
		}

		return allSent;
	}

	std::size_t UdpPacketSender::BroadcastPacket(std::span<const common::net::EndpointKey> endpointKeyList, const void* packetData, int packetSize)
	{
		std::size_t sentCount = 0;

		for (const common::net::EndpointKey& endpointKey : endpointKeyList)
		{
			if (SendPacket(endpointKey, packetData, packetSize))
			{
				++sentCount;
			}
		}

		return sentCount;
	}

	std::size_t UdpPacketSender::BroadcastPacket(std::span<const sockaddr_in> remoteAddressList, const void* packetData, int packetSize)
	{
		std::size_t sentCount = 0;

		for (const sockaddr_in& remoteAddress : remoteAddressList)
		{
			if (SendPacket(remoteAddress, packetData, packetSize))
			{
				++sentCount;
			}
		}

		return sentCount;
	}

	bool UdpPacketSender::SendAccountLoginResponse(const sockaddr_in& remoteAddress, const common::packet::AccountLoginResponsePacket& packet)
	{
		return SendSerializedPacket(*this, remoteAddress, packet);
	}

	bool UdpPacketSender::SendJoinResponse(const sockaddr_in& remoteAddress, PlayerId playerId, RoomId roomId, float spawnX, float spawnY)
	{
		common::packet::JoinResponsePacket packet{};
		packet.playerId = playerId;
		packet.roomId = roomId;
		packet.spawnX = spawnX;
		packet.spawnY = spawnY;

		return SendSerializedPacket(*this, remoteAddress, packet);
	}

	bool UdpPacketSender::SendJoinRoomResponse(const common::net::EndpointKey& endpointKey, RoomId roomId, float spawnX, float spawnY)
	{
		common::packet::JoinRoomResponsePacket packet{};
		packet.roomId = roomId;
		packet.spawnX = spawnX;
		packet.spawnY = spawnY;
		
		return SendSerializedPacket(*this, endpointKey, packet);
	}

	std::size_t UdpPacketSender::BroadcastPlayerJoined(std::span<const common::net::EndpointKey> endpointKeyList, RoomId roomId, PlayerId playerId, float x, float y)
	{
		common::packet::PlayerJoinedPacket packet{};
		packet.playerId = playerId;
		packet.roomId = roomId;
		packet.x = x;
		packet.y = y;

		return BroadcastSerializedPacket(*this, endpointKeyList, packet);
	}

	std::size_t UdpPacketSender::BroadcastPlayerLeft(std::span<const common::net::EndpointKey> endpointKeyList, RoomId roomId, PlayerId playerId)
	{
		common::packet::PlayerLeftPacket packet{};
		packet.playerId = playerId;
		packet.roomId = roomId;

		return BroadcastSerializedPacket(*this, endpointKeyList, packet);
	}

	std::size_t UdpPacketSender::SendPlayerSnapshotTasks(std::span<const protocol::PlayerSnapshotTask> playerSnapshotTaskList)
	{
		std::size_t sentCount = 0;

		for (const protocol::PlayerSnapshotTask& playerSnapshotTask : playerSnapshotTaskList)
		{
			if (SendSerializedPacket(*this, playerSnapshotTask.endpointKey, playerSnapshotTask.snapshotPacket))
			{
				++sentCount;
			}
		}

		return sentCount;
	}

	std::size_t UdpPacketSender::SendBulletSnapshotTasks(std::span<const protocol::BulletSnapshotTask> bulletSnapshotTaskList)
	{
		std::size_t sentCount = 0;

		for (const protocol::BulletSnapshotTask& bulletSnapshotTask : bulletSnapshotTaskList)
		{
			sentCount += BroadcastSerializedPacket(
				*this,
				bulletSnapshotTask.endpointKeyList,
				bulletSnapshotTask.snapshotPacket
			);
		}

		return sentCount;
	}

	std::size_t UdpPacketSender::SendImpactEffectTasks(std::span<const protocol::ImpactEffectTask> impactEffectTaskList)
	{
		std::size_t sentCount = 0;

		for (const protocol::ImpactEffectTask& impactEffectTask : impactEffectTaskList)
		{
			sentCount += BroadcastSerializedPacket(
				*this,
				impactEffectTask.endpointKeyList,
				impactEffectTask.effectPacket
			);
		}

		return sentCount;
	}

	void UdpPacketSender::SetFaultSimulationConfig(const common::net::UdpFaultSimulationConfig& config)
	{
		faultSimulator_.SetConfig(config);
	}

	void UdpPacketSender::ResetFaultSimulation()
	{
		faultSimulator_.Reset();
	}

	std::size_t UdpPacketSender::FlushFaultSimulationPackets()
	{
		common::net::UdpFaultSimulator::PacketList readyPacketList = faultSimulator_.ExtractReadyPackets(common::time::Clock::now());
		std::size_t sentCount = 0;

		for (const common::net::UdpFaultSimulator::Packet& packet : readyPacketList)
		{
			if (SendRawPacket(packet.remoteAddress, packet.packetBuffer.data(), static_cast<int>(packet.packetBuffer.size())))
			{
				++sentCount;
			}
		}

		return sentCount;
	}

	bool UdpPacketSender::SendRawPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize) const
	{
		if (udpTransport_ == nullptr || packetData == nullptr || packetSize <= 0)
		{
			return false;
		}

		return udpTransport_->SendPacket(remoteAddress, packetData, packetSize);
	}
}