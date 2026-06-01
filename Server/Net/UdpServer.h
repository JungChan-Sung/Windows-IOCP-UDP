#pragma once

#include <WinSock2.h>

#include <atomic>
#include <cstddef>
#include <expected>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include <Common/Game/GameTypes.h>
#include <Common/Net/Endpoint.h>
#include <Common/Packet/PacketBuffer.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Diagnostics/ServerMetricsCollector.h>
#include <Server/Diagnostics/ServerStatusReporter.h>
#include <Server/Diagnostics/ServerStatusSnapshot.h>
#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameTickRunner.h>
#include <Server/Game/GameWorld.h>
#include <Server/Net/InvalidPacketLogLimiter.h>
#include <Server/Net/PeerRoomManager.h>
#include <Server/Net/PeerSessionService.h>
#include <Server/Net/PlayerCommandService.h>
#include <Server/Net/SnapshotBroadcastBuilder.h>
#include <Server/Net/UdpIocpTransport.h>
#include <Server/Net/UdpPacketDispatcher.h>
#include <Server/Net/UdpPacketSender.h>

namespace common::log
{
	class ILogger;
}

namespace common::packet
{
	struct InputCommandPacket;
	struct JoinRoomRequestPacket;
}

namespace server::net
{
	class UdpServer
	{
	public:
		enum class StartFailure
		{
			AlreadyRunning,
		};

	public:
		using StartError = std::variant<StartFailure, UdpIocpTransport::StartError, game::GameTickRunner::StartError>;
		using StartResult = std::expected<void, StartError>;

		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;
		using EndpointKey = common::net::EndpointKey;

	private:
		std::atomic<bool> isRunning_ = false;

		UdpIocpTransport udpTransport_;
		game::GameTickRunner gameTickRunner_;

		mutable std::mutex stateMutex_;

		UdpPacketDispatcher packetDispatcher_;
		UdpPacketSender packetSender_;
		SnapshotBroadcastBuilder snapshotBroadcastBuilder_;
		InvalidPacketLogLimiter invalidPacketLogLimiter_;
		diagnostics::ServerMetricsCollector serverMetricsCollector_;
		diagnostics::ServerStatusReporter serverStatusReporter_;

		game::GameSimulation gameSimulation_;
		game::GameWorld gameWorld_;
		PeerRoomManager peerRoomManager_;
		PeerSessionService peerSessionService_;
		PlayerCommandService playerCommandService_;

		common::log::ILogger* logger_ = nullptr;

		server::config::ServerConfig config_{};

		unsigned short port_ = 0;
		std::size_t workerThreadCount_ = 0;

	public:
		UdpServer() = default;
		~UdpServer() noexcept;

		UdpServer(const UdpServer&) = delete;
		UdpServer& operator=(const UdpServer&) = delete;

		UdpServer(UdpServer&&) = delete;
		UdpServer& operator=(UdpServer&&) = delete;

	public:
		[[nodiscard]] static std::string ToString(const StartError& startError);

	public:
		[[nodiscard]] StartResult Start(const server::config::ServerConfig& config);
		[[nodiscard]] StartResult Start(unsigned short port, std::size_t workerThreadCount = 0);
		void Stop() noexcept;

		void AttachLogger(common::log::ILogger& logger) noexcept;
		void DetachLogger() noexcept;

	private:
		void UpdateGameTick();

		void RegisterPacketHandlers();
		[[nodiscard]] UdpPacketDispatcher::DispatchResult DispatchPacket(const sockaddr_in& remoteAddress, const char* packetData, int packetSize);
		[[nodiscard]] UdpPacketDispatcher::DispatchResult DispatchReliablePacket(const sockaddr_in& remoteAddress, const char* packetData, int packetSize);

		[[nodiscard]] std::optional<common::packet::PacketBuffer> BuildReliablePacket(
			PeerState& peerState,
			std::span<const char> serializedGamePacket
		);
		[[nodiscard]] std::optional<common::packet::PacketBuffer> BuildReliableAckPacket(PeerState& peerState);
		[[nodiscard]] std::optional<common::packet::PacketBuffer> BuildReliableJoinRoomResponse(
			PeerState& peerState,
			RoomId roomId,
			float spawnX,
			float spawnY
		);

		void HandleJoinRequest(const sockaddr_in& remoteAddress);
		void HandleInputCommand(const sockaddr_in& remoteAddress, const common::packet::InputCommandPacket& packet);
		void HandleFireRequest(const sockaddr_in& remoteAddress);
		void HandleLeaveRequest(const sockaddr_in& remoteAddress);
		void HandleJoinRoomRequest(const sockaddr_in& remoteAddress, const common::packet::JoinRoomRequestPacket& packet);

		void ProcessReliableResends();
		void ProcessJoinRequest(const sockaddr_in& remoteAddress);
		void ProcessInputCommand(const EndpointKey& endpointKey, const common::packet::InputCommandPacket& packet);
		void ProcessFireRequest(const EndpointKey& endpointKey);
		void ProcessLeaveRequest(const EndpointKey& endpointKey);
		void ProcessJoinRoomRequest(const EndpointKey& endpointKey, const common::packet::JoinRoomRequestPacket& packet);

		void BroadcastPlayerSnapshots();
		void BroadcastBulletSnapshots();
		void BroadcastImpactEffects();

		void BroadcastPlayerJoined(RoomId roomId, PlayerId playerId, float x, float y);
		void BroadcastPlayerLeft(RoomId roomId, PlayerId playerId);

		void RemoveTimedOutPeers();

		void LogDebug(std::string_view message) const;
		void LogInfo(std::string_view message) const;
		void LogWarning(std::string_view message) const;
		void LogError(std::string_view message) const;
		void LogInvalidPacket(const sockaddr_in& remoteAddress, const UdpPacketDispatcher::DispatchResult& dispatchResult);
		void LogServerStatusIfDue();

		[[nodiscard]] diagnostics::ServerStatusSnapshot BuildServerStatusSnapshot() const;

	public:
		[[nodiscard]] const config::ServerConfig& GetConfig() const noexcept
		{
			return config_;
		}

		[[nodiscard]] unsigned short GetPort() const noexcept
		{
			return port_;
		}

		[[nodiscard]] std::size_t GetWorkerThreadCount() const noexcept
		{
			return workerThreadCount_;
		}

		[[nodiscard]] bool IsRunning() const noexcept
		{
			return isRunning_.load();
		}
	};
}

