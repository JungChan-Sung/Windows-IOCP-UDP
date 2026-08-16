#pragma once

#include <WinSock2.h>

#include <cstdint>
#include <span>

#include <Common/Game/GameTypes.h>
#include <Common/Net/Fault/UdpFaultSimulator.h>

#include <Server/Protocol/SnapshotBroadcastTask.h>

namespace common::packet
{
	struct AccountLoginResponsePacket;
}

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
		common::net::UdpFaultSimulator faultSimulator_;

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

		[[nodiscard]] bool SendPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize);
		[[nodiscard]] std::size_t BroadcastPacket(std::span<const sockaddr_in> remoteAddressList, const void* packetData, int packetSize);

		[[nodiscard]] bool SendAccountLoginResponse(const sockaddr_in& remoteAddress, const common::packet::AccountLoginResponsePacket& packet);
		[[nodiscard]] bool SendJoinResponse(const sockaddr_in& remoteAddress, PlayerId playerId, RoomId roomId, float spawnX, float spawnY);
		[[nodiscard]] bool SendJoinRoomResponse(const sockaddr_in& remoteAddress, RoomId roomId, float spawnX, float spawnY);
		[[nodiscard]] std::size_t BroadcastPlayerJoined(std::span<const sockaddr_in> remoteAddressList, RoomId roomId, PlayerId playerId, float x, float y);
		[[nodiscard]] std::size_t BroadcastPlayerLeft(std::span<const sockaddr_in> remoteAddressList, RoomId roomId, PlayerId playerId);

		[[nodiscard]] std::size_t SendPlayerSnapshotTasks(std::span<const protocol::PlayerSnapshotTask> playerSnapshotTaskList);
		[[nodiscard]] std::size_t SendBulletSnapshotTasks(std::span<const protocol::BulletSnapshotTask> bulletSnapshotTaskList);
		[[nodiscard]] std::size_t SendImpactEffectTasks(std::span<const protocol::ImpactEffectTask> impactEffectTaskList);

		void SetFaultSimulationConfig(const common::net::UdpFaultSimulationConfig& config);
		void ResetFaultSimulation();

		[[nodiscard]] std::size_t FlushFaultSimulationPackets();

	private:
		[[nodiscard]] bool SendRawPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize) const;

	public:
		[[nodiscard]] std::size_t GetFaultSimulationPendingPacketCount() const noexcept
		{
			return faultSimulator_.GetPendingPacketCount();
		}
	};
}

