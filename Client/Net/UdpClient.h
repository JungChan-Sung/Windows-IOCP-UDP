#pragma once

#include <atomic>
#include <cstdint>
#include <chrono>

#include <Common/Net/Socket.h>
#include <Common/Game/InputFlags.h>
#include <Common/Game/GameTypes.h>

#include <Client/Config/ClientConfigDefaults.h>
#include <Client/Net/ClientPacketDispatcher.h>
#include <Client/Net/SnapshotChunkAssembler.h>
#include <Client/Net/UdpSocketTransport.h>

namespace common::packet
{
	struct JoinResponsePacket;
	struct JoinRoomResponsePacket;
	struct PlayerJoinedPacket;
	struct PlayerLeftPacket;
	struct PlayerSnapshotPacket;
	struct BulletSnapshotPacket;
	struct ImpactEffectPacket;
}

namespace client::game
{
	class ClientWorld;
}

namespace client::net
{
	class UdpClient
	{
	public:
		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;

	private:
		using ClientWorldType = game::ClientWorld;

	private:
		UdpSocketTransport udpTransport_;

		std::atomic<bool> isRunning_ = false;
		ClientWorldType* world_ = nullptr;
		std::uint32_t inputSequence_ = 0;
		std::chrono::milliseconds snapshotAssemblyTimeout_ = config::defaultSnapshotAssemblyTimeout;
		bool enableChunkAssemblerDebugTests_ = config::defaultEnableChunkAssemblerDebugTests;
		ClientPacketDispatcher packetDispatcher_;
		SnapshotChunkAssembler snapshotChunkAssembler_;

	public:
		UdpClient() = default;
		~UdpClient() noexcept;

		UdpClient(const UdpClient&) = delete;
		UdpClient& operator=(const UdpClient&) = delete;
		
		UdpClient(UdpClient&&) = delete;
		UdpClient& operator=(UdpClient&&) = delete;

	public:
		[[nodiscard]] bool Start(const char* serverIp, unsigned short serverPort, ClientWorldType& world);
		void Stop() noexcept;

		[[nodiscard]] bool SendJoinRequest();
		[[nodiscard]] bool SendInputCommand(common::game::InputFlags inputFlags, std::uint32_t& inputSequence);
		[[nodiscard]] bool SendFireRequest();
		[[nodiscard]] bool SendLeaveRequest();
		[[nodiscard]] bool SendJoinRoomRequest(RoomId roomId);

	private:
		void RegisterPacketHandlers();

		void HandlePacket(const char* packetData, int packetSize);
		void HandleJoinResponse(const common::packet::JoinResponsePacket& packet);
		void HandleJoinRoomResponse(const common::packet::JoinRoomResponsePacket& packet);
		void HandlePlayerJoined(const common::packet::PlayerJoinedPacket& packet);
		void HandlePlayerLeft(const common::packet::PlayerLeftPacket& packet);
		void HandlePlayerSnapshot(const common::packet::PlayerSnapshotPacket& packet);
		void HandleBulletSnapshot(const common::packet::BulletSnapshotPacket& packet);
		void HandleImpactEffectPacket(const common::packet::ImpactEffectPacket & packet);

	public:
		void SetSnapshotAssemblyTimeout(std::chrono::milliseconds snapshotAssemblyTimeout) noexcept;

		void SetEnableChunkAssemblerDebugTests(bool enableChunkAssemblerDebugTests) noexcept
		{
			enableChunkAssemblerDebugTests_ = enableChunkAssemblerDebugTests;
		}
	};
}

