#pragma once

#include <WinSock2.h>

#include <cstdint>
#include <span>

#include <Common/Game/GameTypes.h>

#include <Server/Net/SnapshotBroadcastTask.h>

namespace server::net
{
	class UdpIocpTransport;

	class UdpPacketSender
	{
	public:
		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;

	private:
		UdpIocpTransport* udpTransport_ = nullptr;

	public:
		UdpPacketSender() = default;
		~UdpPacketSender() noexcept = default;

		UdpPacketSender(const UdpPacketSender&) = delete;
		UdpPacketSender& operator=(const UdpPacketSender&) = delete;

		UdpPacketSender(UdpPacketSender&&) = delete;
		UdpPacketSender& operator=(UdpPacketSender&&) = delete;

	public:
		void AttachTransport(UdpIocpTransport& udpTransport) noexcept;
		void DetachTransport() noexcept;

		[[nodiscard]] bool SendPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize) const;
		[[nodiscard]] std::size_t BroadcastPacket(std::span<const sockaddr_in> remoteAddressList, const void* packetData, int packetSize) const;

		[[nodiscard]] bool SendJoinResponse(const sockaddr_in& remoteAddress, PlayerId playerId, float spawnX, float spawnY) const;
		[[nodiscard]] bool SendJoinRoomResponse(const sockaddr_in& remoteAddress, RoomId roomId, float spawnX, float spawnY) const;
		[[nodiscard]] std::size_t BroadcastPlayerJoined(std::span<const sockaddr_in> remoteAddressList, RoomId roomId, PlayerId playerId, float x, float y) const;
		[[nodiscard]] std::size_t BroadcastPlayerLeft(std::span<const sockaddr_in> remoteAddressList, RoomId roomId, PlayerId playerId) const;

		[[nodiscard]] std::size_t SendPlayerSnapshotTasks(std::span<const PlayerSnapshotTask> playerSnapshotTaskList) const;
		[[nodiscard]] std::size_t SendBulletSnapshotTasks(std::span<const BulletSnapshotTask> bulletSnapshotTaskList) const;
		[[nodiscard]] std::size_t SendImpactEffectTasks(std::span<const ImpactEffectTask> impactEffectTaskList) const;
	};
}

