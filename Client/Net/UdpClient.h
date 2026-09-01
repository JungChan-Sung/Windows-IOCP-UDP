#pragma once

#include <atomic>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <Common/Net/Auth/PacketAuthentication.h>
#include <Common/Net/Reliable/ReliableUdpSession.h>
#include <Common/Game/InputFlags.h>
#include <Common/Game/GameTypes.h>
#include <Common/Packet/Control/ControlPacket.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Time/TimeTypes.h>

#include <Client/Config/ClientTransportType.h>
#include <Client/Config/ClientConfigDefaults.h>
#include <Client/Net/AccountLoginState.h>
#include <Client/Net/ClientPacketDispatcher.h>
#include <Client/Net/SnapshotChunkAssembler.h>
#include <Client/Net/UdpIocpTransport.h>
#include <Client/Net/UdpSocketTransport.h>

namespace client::game
{
	class ClientWorld;
}

namespace common::log
{
	class ILogger;
}

namespace common::packet
{
	struct AccountLoginResponsePacket;
	struct JoinResponsePacket;
	struct JoinRoomResponsePacket;
	struct LeaveResponsePacket;
	struct PlayerJoinedPacket;
	struct PlayerLeftPacket;
	struct PlayerSnapshotPacket;
	struct BulletSnapshotPacket;
	struct ImpactEffectPacket;
}

namespace client::net
{
	class UdpClient
	{
	public:
		enum class StartFailure
		{
			AlreadyRunning,
			NotRunning,
			InvalidTransportType,
		};

	public:
		using StartError = std::variant<StartFailure, UdpSocketTransport::StartError, UdpIocpTransport::StartError>;
		using StartResult = std::expected<void, StartError>;

		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;
		
		using TimePoint = common::time::TimePoint;
		using Duration = common::time::Duration;

		using ServerDisconnectReason = common::packet::ServerDisconnectReason;

		using AccountLoginRequestId = AccountLoginState::RequestId;
		using AccountLoginSnapshot = AccountLoginState::Snapshot;

	private:
		using ClientWorldType = game::ClientWorld;

	private:
		UdpSocketTransport socketTransport_;
		UdpIocpTransport iocpTransport_;

		common::log::ILogger* logger_ = nullptr;

		config::ClientTransportType transportType_ = config::ClientTransportType::Socket;
		std::size_t iocpWorkerThreadCount_ = config::defaultIocpWorkerThreadCount;
		std::size_t iocpRecvContextCount_ = config::defaultIocpRecvContextCount;

		std::atomic<bool> isRunning_ = false;
		std::atomic<bool> leaveResponseReceived_ = false;
		std::atomic<ServerDisconnectReason> serverDisconnectReason_ = ServerDisconnectReason::None;
		std::atomic<common::net::PacketAuthenticationSequence> nextPacketAuthenticationSequence_ = 1;
		std::atomic<Duration::rep> lastServerPacketReceiveTimeCount_ = 0;

		ClientWorldType* world_ = nullptr;
		std::uint32_t inputSequence_ = 0;
		common::time::Milliseconds snapshotAssemblyTimeout_ = config::defaultSnapshotAssemblyTimeout;

		AccountLoginState accountLoginState_;
		ClientPacketDispatcher packetDispatcher_;
		SnapshotChunkAssembler snapshotChunkAssembler_;

		std::mutex reliableSessionMutex_;
		common::net::ReliableUdpSession reliableSession_;

	public:
		UdpClient() = default;
		~UdpClient() noexcept;

		UdpClient(const UdpClient&) = delete;
		UdpClient& operator=(const UdpClient&) = delete;
		
		UdpClient(UdpClient&&) = delete;
		UdpClient& operator=(UdpClient&&) = delete;

	public:
		[[nodiscard]] static std::string ToString(const StartError& startError);

	public:
		[[nodiscard]] StartResult Start(const char* serverIp, unsigned short serverPort, ClientWorldType& world);
		[[nodiscard]] StartResult RestartTransport(const char* serverIp, unsigned short serverPort);
		void Stop() noexcept;

		void AttachLogger(common::log::ILogger& logger) noexcept;
		void DetachLogger() noexcept;

		[[nodiscard]] AccountLoginRequestId BeginAccountLogin(
			std::string loginName,
			std::string passwordHash,
			common::time::Milliseconds retryInterval
		);
		void ProcessAccountLogin();
		void ResetAccountLogin();

		[[nodiscard]] bool SendJoinRequest();
		[[nodiscard]] std::optional<std::uint32_t> SendInputCommand(common::game::InputFlags inputFlags);
		[[nodiscard]] bool SendFireRequest();
		[[nodiscard]] bool SendKeepAlive();
		[[nodiscard]] bool SendLeaveRequest();
		[[nodiscard]] bool SendJoinRoomRequest(RoomId roomId);

		void ProcessReliableResends();

	private:
		[[nodiscard]] StartResult StartTransport(const char* serverIp, unsigned short serverPort);
		void StopTransport() noexcept;
		void ResetTransportSessionState() noexcept;

		[[nodiscard]] std::optional<common::packet::PacketBuffer> BuildAuthenticatedPacket(common::packet::ConstPacketSpan packet);

		[[nodiscard]] bool SendPacket(const void* packetData, int packetSize);
		[[nodiscard]] bool SendAccountLoginRequest(const common::packet::AccountLoginRequestPacket& packet);
		[[nodiscard]] bool SendSerializedPacket(common::packet::ConstPacketSpan serializedPacket);
		[[nodiscard]] bool SendReliablePacket(common::packet::ConstPacketSpan serializedGamePacket);
		[[nodiscard]] bool SendReliableAckPacket();

		void RegisterPacketHandlers();

		void RecordServerPacketReceiveTime(TimePoint currentTime) noexcept;

		void HandlePacket(const char* packetData, int packetSize);
		void HandleReliablePacket(const char* packetData, int packetSize);
		void HandleAccountLoginResponse(const common::packet::AccountLoginResponsePacket& packet);
		void HandleJoinResponse(const common::packet::JoinResponsePacket& packet);
		void HandleLeaveResponse(const common::packet::LeaveResponsePacket& packet);
		void HandleJoinRoomResponse(const common::packet::JoinRoomResponsePacket& packet);
		void HandlePlayerJoined(const common::packet::PlayerJoinedPacket& packet);
		void HandlePlayerLeft(const common::packet::PlayerLeftPacket& packet);
		void HandlePlayerSnapshot(const common::packet::PlayerSnapshotPacket& packet);
		void HandleBulletSnapshot(const common::packet::BulletSnapshotPacket& packet);
		void HandleImpactEffectPacket(const common::packet::ImpactEffectPacket& packet);
		void HandleServerDisconnect(const common::packet::ServerDisconnectPacket& packet);

		void LogDebug(std::string_view message) const;
		void LogInfo(std::string_view message) const;
		void LogWarning(std::string_view message) const;
		void LogError(std::string_view message) const;

	public:
		void SetTransportConfig(
			config::ClientTransportType transportType,
			std::size_t iocpWorkerThreadCount,
			std::size_t iocpRecvContextCount
		) noexcept;

		void SetSnapshotAssemblyTimeout(common::time::Milliseconds snapshotAssemblyTimeout) noexcept;

		[[nodiscard]] AccountLoginSnapshot GetAccountLoginSnapshot() const;

		[[nodiscard]] bool HasServerReceiveTimedOut(TimePoint currentTime, Duration timeout) const noexcept;

		[[nodiscard]] bool HasReceivedLeaveResponse() const noexcept
		{
			return leaveResponseReceived_.load();
		}

		[[nodiscard]] ServerDisconnectReason GetServerDisconnectReason() const noexcept
		{
			return serverDisconnectReason_.load();
		}

		[[nodiscard]] bool HasReceivedServerDisconnect() const noexcept
		{
			return GetServerDisconnectReason() != ServerDisconnectReason::None;
		}
	};
}

