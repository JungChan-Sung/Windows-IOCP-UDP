#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <string_view>

#include <Common/Game/InputFlags.h>
#include <Common/Game/GameTypes.h>

#include <Client/Config/ClientTransportType.h>
#include <Client/Config/ClientConfigDefaults.h>
#include <Client/Net/ClientPacketDispatcher.h>
#include <Client/Net/SnapshotChunkAssembler.h>
#include <Client/Net/UdpIocpTransport.h>
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
		enum class StartError
		{
			AlreadyRunning,
			InvalidTransportType,

			SocketTransportAlreadyRunning,
			SocketTransportInvalidCallback,
			SocketTransportCreateSocketFailed,
			SocketTransportBindSocketFailed,
			SocketTransportConfigureSocketFailed,
			SocketTransportSetServerAddressFailed,
			SocketTransportStartRecvThreadFailed,

			IocpTransportStartFailed,
		};

	public:
		using StartResult = std::expected<void, StartError>;

		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;

	private:
		using ClientWorldType = game::ClientWorld;

	private:
		UdpSocketTransport socketTransport_;
		UdpIocpTransport iocpTransport_;

		config::ClientTransportType transportType_ = config::ClientTransportType::Socket;
		std::size_t iocpWorkerThreadCount_ = config::defaultIocpWorkerThreadCount;
		std::size_t iocpRecvContextCount_ = config::defaultIocpRecvContextCount;

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
		[[nodiscard]] static std::string_view ToString(StartError startError) noexcept;

		[[nodiscard]] static StartError ToStartError(UdpSocketTransport::StartError startError) noexcept;

	public:
		[[nodiscard]] StartResult Start(const char* serverIp, unsigned short serverPort, ClientWorldType& world);
		void Stop() noexcept;

		[[nodiscard]] bool SendJoinRequest();
		[[nodiscard]] bool SendInputCommand(common::game::InputFlags inputFlags, std::uint32_t& inputSequence);
		[[nodiscard]] bool SendFireRequest();
		[[nodiscard]] bool SendLeaveRequest();
		[[nodiscard]] bool SendJoinRoomRequest(RoomId roomId);

	private:
		[[nodiscard]] StartResult StartTransport(const char* serverIp, unsigned short serverPort);
		void StopTransport() noexcept;
		[[nodiscard]] bool SendPacket(const void* packetData, int packetSize);

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
		void SetTransportConfig(
			config::ClientTransportType transportType,
			std::size_t iocpWorkerThreadCount,
			std::size_t iocpRecvContextCount
		) noexcept;

		void SetSnapshotAssemblyTimeout(std::chrono::milliseconds snapshotAssemblyTimeout) noexcept;

		void SetEnableChunkAssemblerDebugTests(bool enableChunkAssemblerDebugTests) noexcept
		{
			enableChunkAssemblerDebugTests_ = enableChunkAssemblerDebugTests;
		}
	};
}

