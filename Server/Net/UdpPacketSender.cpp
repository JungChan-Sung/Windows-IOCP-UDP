#include "UdpPacketSender.h"

#include <optional>

#include <Common/Packet/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>

#include <Server/Net/UdpIocpTransport.h>

namespace
{
	template <typename TPacket>
	[[nodiscard]] bool SendSerializedPacket(
		const server::net::UdpPacketSender& packetSender,
		const sockaddr_in& remoteAddress,
		const TPacket& packet
	)
	{
		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return false;
		}

		return packetSender.SendPacket(
			remoteAddress,
			packetBuffer->data(),
			static_cast<int>(packetBuffer->size())
		);
	}

	template <typename TPacket>
	[[nodiscard]] std::size_t BroadcastSerializedPacket(
		const server::net::UdpPacketSender& packetSender,
		std::span<const sockaddr_in> remoteAddressList,
		const TPacket& packet
	)
	{
		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return 0;
		}

		return packetSender.BroadcastPacket(
			remoteAddressList,
			packetBuffer->data(),
			static_cast<int>(packetBuffer->size())
		);
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

	bool UdpPacketSender::SendPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize) const
	{
		if (udpTransport_ == nullptr)
		{
			return false;
		}

		return udpTransport_->SendPacket(remoteAddress, packetData, packetSize);
	}

	std::size_t UdpPacketSender::BroadcastPacket(std::span<const sockaddr_in> remoteAddressList, const void* packetData, int packetSize) const
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

	bool UdpPacketSender::SendJoinResponse(const sockaddr_in& remoteAddress, PlayerId playerId, float spawnX, float spawnY) const
	{
		common::packet::JoinResponsePacket packet{};
		packet.playerId = playerId;
		packet.spawnX = spawnX;
		packet.spawnY = spawnY;

		return SendSerializedPacket(*this, remoteAddress, packet);
	}

	bool UdpPacketSender::SendJoinRoomResponse(const sockaddr_in& remoteAddress, RoomId roomId, float spawnX, float spawnY) const
	{
		common::packet::JoinRoomResponsePacket packet{};
		packet.roomId = roomId;
		packet.spawnX = spawnX;
		packet.spawnY = spawnY;

		return SendSerializedPacket(*this, remoteAddress, packet);
	}

	std::size_t UdpPacketSender::BroadcastPlayerJoined(std::span<const sockaddr_in> remoteAddressList, RoomId roomId, PlayerId playerId, float x, float y) const
	{
		common::packet::PlayerJoinedPacket packet{};
		packet.playerId = playerId;
		packet.roomId = roomId;
		packet.x = x;
		packet.y = y;

		return BroadcastSerializedPacket(*this, remoteAddressList, packet);
	}

	std::size_t UdpPacketSender::BroadcastPlayerLeft(std::span<const sockaddr_in> remoteAddressList, RoomId roomId, PlayerId playerId) const
	{
		common::packet::PlayerLeftPacket packet{};
		packet.playerId = playerId;
		packet.roomId = roomId;

		return BroadcastSerializedPacket(*this, remoteAddressList, packet);
	}

	std::size_t UdpPacketSender::SendPlayerSnapshotTasks(std::span<const PlayerSnapshotTask> playerSnapshotTaskList) const
	{
		std::size_t sentCount = 0;

		for (const PlayerSnapshotTask& playerSnapshotTask : playerSnapshotTaskList)
		{
			if (SendSerializedPacket(*this, playerSnapshotTask.remoteAddress, playerSnapshotTask.snapshotPacket))
			{
				++sentCount;
			}
		}

		return sentCount;
	}

	std::size_t UdpPacketSender::SendBulletSnapshotTasks(std::span<const BulletSnapshotTask> bulletSnapshotTaskList) const
	{
		std::size_t sentCount = 0;

		for (const BulletSnapshotTask& bulletSnapshotTask : bulletSnapshotTaskList)
		{
			sentCount += BroadcastSerializedPacket(
				*this,
				bulletSnapshotTask.remoteAddressList,
				bulletSnapshotTask.snapshotPacket
			);
		}

		return sentCount;
	}

	std::size_t UdpPacketSender::SendImpactEffectTasks(std::span<const ImpactEffectTask> impactEffectTaskList) const
	{
		std::size_t sentCount = 0;

		for (const ImpactEffectTask& impactEffectTask : impactEffectTaskList)
		{
			sentCount += BroadcastSerializedPacket(
				*this,
				impactEffectTask.remoteAddressList,
				impactEffectTask.effectPacket
			);
		}

		return sentCount;
	}
}