#pragma once

#include <WinSock2.h>

#include <atomic>
#include <cstddef>
#include <expected>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <Common/Game/GameTypes.h>
#include <Common/Net/Endpoint.h>
#include <Common/Packet/PacketBuffer.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Diagnostics/InvalidPacketLogLimiter.h>
#include <Server/Diagnostics/ServerMetricsCollector.h>
#include <Server/Diagnostics/ServerStatusReporter.h>
#include <Server/Diagnostics/ServerStatusSnapshot.h>
#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameTickRunner.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/MatchHistoryTracker.h>
#include <Server/Net/ReliableUdpSessionRegistry.h>
#include <Server/Net/UdpIocpTransport.h>
#include <Server/Net/UdpPacketSender.h>
#include <Server/Service/AccountLoginAdmissionService.h>
#include <Server/Service/AuthenticatedAccountRegistry.h>
#include <Server/Service/PeerRoomManager.h>
#include <Server/Service/PeerSessionService.h>
#include <Server/Service/PlayerCommandService.h>
#include <Server/Protocol/SnapshotBroadcastBuilder.h>
#include <Server/Protocol/SnapshotBroadcastContext.h>
#include <Server/Protocol/UdpPacketDispatcher.h>

namespace common::log
{
	class ILogger;
}

namespace common::packet
{
	struct AccountLoginRequestPacket;
	struct InputCommandPacket;
	struct JoinRequestPacket;
	struct JoinRoomRequestPacket;
}

namespace server::protocol
{
	class AccountLoginPacketHandler;
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

		protocol::UdpPacketDispatcher packetDispatcher_;
		protocol::SnapshotBroadcastBuilder snapshotBroadcastBuilder_;
		protocol::AccountLoginPacketHandler* accountLoginPacketHandler_ = nullptr;

		UdpPacketSender packetSender_;
		ReliableUdpSessionRegistry reliableUdpSessionRegistry_;

		diagnostics::InvalidPacketLogLimiter invalidPacketLogLimiter_;
		diagnostics::ServerMetricsCollector serverMetricsCollector_;
		diagnostics::ServerStatusReporter serverStatusReporter_;

		game::GameSimulation gameSimulation_;
		game::GameWorld gameWorld_;
		game::MatchHistoryTracker matchHistoryTracker_;

		service::AuthenticatedAccountRegistry authenticatedAccountRegistry_;
		service::AccountLoginAdmissionService accountLoginAdmissionService_;
		service::PeerRoomManager peerRoomManager_;
		service::PeerSessionService peerSessionService_;
		service::PlayerCommandService playerCommandService_;

		common::log::ILogger* logger_ = nullptr;

		config::ServerConfig config_{};

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
		[[nodiscard]] StartResult Start(const config::ServerConfig& config);
		[[nodiscard]] StartResult Start(unsigned short port, std::size_t workerThreadCount = 0);
		void Stop() noexcept;

		void AttachLogger(common::log::ILogger& logger) noexcept;
		void DetachLogger() noexcept;

		void AttachAccountLoginPacketHandler(protocol::AccountLoginPacketHandler& accountLoginPacketHandler) noexcept;
		void DetachAccountLoginPacketHandler() noexcept;

		[[nodiscard]] game::CompletedMatchList ExtractCompletedMatches();
		[[nodiscard]] diagnostics::ServerStatusSnapshot CaptureStatusSnapshot() const;

	private:
		void UpdateGameTick();

		void CommitProcessedInputSequences() noexcept;

		[[nodiscard]] game::PlayerSimulationContextList BuildPlayerSimulationContextList() const;
		[[nodiscard]] protocol::SnapshotBroadcastContext BuildSnapshotBroadcastContext() const;

		void RegisterPacketHandlers();
		[[nodiscard]] protocol::UdpPacketDispatcher::DispatchResult DispatchPacket(const EndpointKey& endpointKey, const char* packetData, int packetSize);
		[[nodiscard]] protocol::UdpPacketDispatcher::DispatchResult DispatchReliablePacket(const EndpointKey& endpointKey, const char* packetData, int packetSize);

		[[nodiscard]] std::optional<common::packet::PacketBuffer> BuildReliableJoinRoomResponse(
			const EndpointKey& endpointKey,
			const service::PeerSessionService::RoomChangeResult& roomChangeResult
		);
		[[nodiscard]] std::optional<common::packet::PacketBuffer> BuildReliableLeaveResponse(const EndpointKey& endpointKey);

		void HandleJoinRequest(const EndpointKey& endpointKey, const common::packet::JoinRequestPacket& packet);
		void HandleInputCommand(const EndpointKey& endpointKey, const common::packet::InputCommandPacket& packet);
		void HandleFireRequest(const EndpointKey& endpointKey);
		void HandleKeepAlive(const EndpointKey& endpointKey);
		void HandleLeaveRequest(const EndpointKey& endpointKey);
		void HandleJoinRoomRequest(const EndpointKey& endpointKey, const common::packet::JoinRoomRequestPacket& packet);
		void HandleAccountLoginRequest(const EndpointKey& endpointKey, const common::packet::AccountLoginRequestPacket& packet);

		void ProcessReliableResends();
		void ProcessJoinRequest(const EndpointKey& endpointKey, const common::packet::JoinRequestPacket& packet);
		void ProcessInputCommand(const EndpointKey& endpointKey, const common::packet::InputCommandPacket& packet);
		void ProcessFireRequest(const EndpointKey& endpointKey);
		void ProcessLeaveRequest(const EndpointKey& endpointKey);
		void ProcessJoinRoomRequest(const EndpointKey& endpointKey, const common::packet::JoinRoomRequestPacket& packet);
		void ProcessAccountLoginResponses();

		void BroadcastSnapshots();

		void BroadcastPlayerJoined(RoomId roomId, PlayerId playerId, float x, float y);
		void BroadcastPlayerLeft(RoomId roomId, PlayerId playerId);

		void RemoveTimedOutPeers();

		void LogDebug(std::string_view message) const;
		void LogInfo(std::string_view message) const;
		void LogWarning(std::string_view message) const;
		void LogError(std::string_view message) const;
		void LogInvalidPacket(const EndpointKey& endpointKey, const protocol::UdpPacketDispatcher::DispatchResult& dispatchResult);
		void LogServerStatusIfDue();

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

