#include "UdpServer.h"

#include <WS2tcpip.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <variant>

#include <Common/Log/ILogger.h>
#include <Common/Packet/GamePacket.h>
#include <Common/Packet/PacketReliability.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Packet/ReliableUdpPacketBuilder.h>
#include <Common/String/StringFormat.h>

#include <Server/Config/ServerConfigValidator.h>
#include <Server/Net/PacketPayloadValidator.h>

namespace
{
	template <typename TObject>
	using AddressOnlyPacketHandler = void (TObject::*)(const sockaddr_in&);

	template <typename TObject, typename TPacket>
	using AddressTypedPacketHandler = void (TObject::*)(const sockaddr_in&, const TPacket&);

	template <typename TObject>
	void RegisterAddressOnlyPacketHandler(
		server::net::UdpPacketDispatcher& packetDispatcher,
		common::packet::PacketType packetType,
		TObject& object,
		int expectedPacketSize,
		AddressOnlyPacketHandler<TObject> handler
	)
	{
		using DispatchStatus = server::net::UdpPacketDispatcher::DispatchStatus;
		using PacketProcessResult = server::net::UdpPacketDispatcher::PacketProcessResult;

		packetDispatcher.RegisterHandler(
			packetType,
			expectedPacketSize,
			[&object, handler](const sockaddr_in& remoteAddress, const char*, int)
			{
				std::invoke(handler, object, remoteAddress);
				return PacketProcessResult{ DispatchStatus::Succeeded, 0 };
			}
		);
	}

	template <typename TObject, typename TPacket, typename TValidator>
	void RegisterTypedPacketHandler(
		server::net::UdpPacketDispatcher& packetDispatcher,
		common::packet::PacketType packetType,
		TObject& object,
		TValidator validator,
		AddressTypedPacketHandler<TObject, TPacket> handler
	)
	{
		using Packet = std::remove_cvref_t<TPacket>;
		using DispatchStatus = server::net::UdpPacketDispatcher::DispatchStatus;
		using PacketProcessResult = server::net::UdpPacketDispatcher::PacketProcessResult;
		using PayloadValidationStatus = server::net::PacketPayloadValidator::PayloadValidationStatus;

		packetDispatcher.RegisterHandler(
			packetType,
			common::packet::packetExpectedSize<Packet>,
			[&object, validator, handler](const sockaddr_in& remoteAddress, const char* packetData, int packetSize)
			{
				std::optional<Packet> packet = common::packet::DeserializePacket<Packet>(packetData, packetSize);
				if (!packet.has_value())
				{
					return PacketProcessResult{ DispatchStatus::InvalidPacketPayload, 0 };
				}

				const PayloadValidationStatus validationStatus = std::invoke(validator, *packet);
				if (validationStatus != PayloadValidationStatus::Succeeded)
				{
					return PacketProcessResult{
						DispatchStatus::InvalidPacketPayload,
						static_cast<int>(validationStatus)
					};
				}

				std::invoke(handler, object, remoteAddress, *packet);
				return PacketProcessResult{ DispatchStatus::Succeeded, 0 };
			}
		);
	}

	[[nodiscard]] std::string FormatEndpoint(const sockaddr_in& remoteAddress)
	{
		std::array<char, INET_ADDRSTRLEN> ipAddressBuffer{};

		const char* ipAddress = ::inet_ntop(
			AF_INET,
			&remoteAddress.sin_addr,
			ipAddressBuffer.data(),
			static_cast<socklen_t>(ipAddressBuffer.size())
		);

		std::ostringstream stream;
		stream << ((ipAddress != nullptr) ? ipAddress : "unknown")
			<< ":"
			<< ::ntohs(remoteAddress.sin_port);

		return stream.str();
	}
}

namespace server::net
{
	UdpServer::~UdpServer() noexcept
	{
		Stop();
	}

	std::string UdpServer::ToString(const StartError& startError)
	{
		return std::visit(
			[](const auto& error) -> std::string
			{
				using ErrorType = std::remove_cvref_t<decltype(error)>;

				if constexpr (std::is_same_v<ErrorType, StartFailure>)
				{
					switch (error)
					{
					case StartFailure::AlreadyRunning:
						return "AlreadyRunning";

					default:
						return "Unknown";
					}
				}
				else if constexpr (std::is_same_v<ErrorType, UdpIocpTransport::StartError>)
				{
					return common::string::FormatScopedName("UdpTransport", UdpIocpTransport::ToString(error));
				}
				else if constexpr (std::is_same_v<ErrorType, game::GameTickRunner::StartError>)
				{
					return common::string::FormatScopedName("GameTickRunner", game::GameTickRunner::ToString(error));
				}
				else
				{
					return "Unknown";
				}
			},
			startError
		);
	}

	UdpServer::StartResult UdpServer::Start(const server::config::ServerConfig& config)
	{
		if (isRunning_.load())
		{
			LogWarning("UdpServer start ignored because server is already running.");
			return std::unexpected(StartError{ StartFailure::AlreadyRunning });
		}

		config_ = config;

		const std::vector<server::config::ServerConfigWarning> warningList
			= server::config::ServerConfigValidator::ValidateAndNormalize(config_);

		for (const server::config::ServerConfigWarning& warning : warningList)
		{
			LogWarning(warning.message);
		}

		invalidPacketLogLimiter_.Reset();
		serverMetricsCollector_.Reset();

		serverStatusReporter_.SetEnabled(config_.diagnostics.enableStatusLog);
		serverStatusReporter_.SetReportInterval(config_.diagnostics.statusLogInterval);
		serverStatusReporter_.Reset();

		port_ = config_.network.port;
		workerThreadCount_ = config_.network.workerThreadCount;

		{
			std::ostringstream stream;
			stream << "UdpServer starting. Port=" << port_
				<< ", WorkerThreadCount=" << workerThreadCount_
				<< ", RecvContextCount=" << config_.network.recvContextCount;
			LogInfo(stream.str());
		}

		RegisterPacketHandlers();

		const UdpIocpTransport::StartResult transportStartResult = udpTransport_.Start(
			config_.network.port,
			config_.network.workerThreadCount,
			config_.network.recvContextCount,
			[this](const sockaddr_in& remoteAddress, const char* packetData, int packetSize)
			{
				if (!isRunning_.load())
				{
					return;
				}

				serverMetricsCollector_.IncrementReceivedPacketCount();

				const UdpPacketDispatcher::DispatchResult dispatchResult = DispatchPacket(
					remoteAddress,
					packetData,
					packetSize
				);

				if (dispatchResult.status != UdpPacketDispatcher::DispatchStatus::Succeeded)
				{
					serverMetricsCollector_.IncrementInvalidPacketDropCount();
					LogInvalidPacket(remoteAddress, dispatchResult);
				}
			}
		);
		if (!transportStartResult.has_value())
		{
			std::ostringstream stream;
			stream << "UdpServer failed to start UDP IOCP transport. Error=" << UdpIocpTransport::ToString(transportStartResult.error());
			LogError(stream.str());

			Stop();
			return std::unexpected(StartError{ transportStartResult.error() });
		}

		packetSender_.AttachTransport(udpTransport_);

		isRunning_.store(true);

		const game::GameTickRunner::StartResult gameTickRunnerStartResult = gameTickRunner_.Start(
			config_.tick.tickInterval,
			[this]()
			{
				UpdateGameTick();
			}
		);
		if (!gameTickRunnerStartResult.has_value())
		{
			std::ostringstream stream;
			stream << "UdpServer failed to start game tick runner. Error=" << game::GameTickRunner::ToString(gameTickRunnerStartResult.error());
			LogError(stream.str());

			Stop();
			return std::unexpected(StartError{ gameTickRunnerStartResult.error() });
		}

		LogInfo("UdpServer started.");
		return {};
	}

	UdpServer::StartResult UdpServer::Start(unsigned short port, std::size_t workerThreadCount)
	{
		server::config::ServerConfig config{};
		config.network.port = port;
		config.network.workerThreadCount = workerThreadCount;

		return Start(config);
	}

	void UdpServer::Stop() noexcept
	{
		const bool wasRunning = isRunning_.exchange(false);

		if (wasRunning)
		{
			LogInfo("UdpServer stopping.");
		}

		gameTickRunner_.Stop();

		udpTransport_.Stop();
		packetSender_.DetachTransport();
		packetDispatcher_.Clear();

		{
			std::scoped_lock lock(stateMutex_);

			peerRoomManager_.Clear();
			gameWorld_.Clear();
		}

		invalidPacketLogLimiter_.Reset();
		serverMetricsCollector_.Reset();
		serverStatusReporter_.Reset();

		port_ = 0;
		workerThreadCount_ = 0;

		if (wasRunning)
		{
			LogInfo("UdpServer stopped.");
		}
	}

	void UdpServer::AttachLogger(common::log::ILogger& logger) noexcept
	{
		logger_ = &logger;
	}

	void UdpServer::DetachLogger() noexcept
	{
		logger_ = nullptr;
	}

	void UdpServer::UpdateGameTick()
	{
		if (!isRunning_.load())
		{
			return;
		}

		{
			std::scoped_lock lock(stateMutex_);

			gameSimulation_.UpdatePlayers(
				config_.tick.fixedDeltaSeconds,
				peerRoomManager_.GetPeerTable(),
				gameWorld_
			);
			gameSimulation_.UpdateBullets(
				config_.tick.fixedDeltaSeconds,
				peerRoomManager_.GetPeerTable(),
				gameWorld_,
				config_.gameRule
			);
			gameSimulation_.UpdateRespawns(
				config_.tick.fixedDeltaSeconds,
				peerRoomManager_.GetPeerTable(),
				gameWorld_,
				config_.gameRule
			);
			gameSimulation_.UpdatePlayerTimers(
				config_.tick.fixedDeltaSeconds,
				gameWorld_
			);

			gameWorld_.AdvanceServerTick();
		}

		RemoveTimedOutPeers();
		ProcessReliableResends();

		BroadcastPlayerSnapshots();
		BroadcastBulletSnapshots();
		BroadcastImpactEffects();

		LogServerStatusIfDue();
	}

	void UdpServer::RegisterPacketHandlers()
	{
		packetDispatcher_.Clear();

		RegisterAddressOnlyPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::JoinRequest,
			*this,
			common::packet::packetExpectedSize<common::packet::JoinRequestPacket>,
			&UdpServer::HandleJoinRequest
		);

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::InputCommand,
			*this,
			&PacketPayloadValidator::ValidateInputCommandPacket,
			&UdpServer::HandleInputCommand
		);

		RegisterAddressOnlyPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::FireRequest,
			*this,
			common::packet::packetExpectedSize<common::packet::FireRequestPacket>,
			&UdpServer::HandleFireRequest
		);

		RegisterAddressOnlyPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::LeaveRequest,
			*this,
			common::packet::packetExpectedSize<common::packet::LeaveRequestPacket>,
			&UdpServer::HandleLeaveRequest
		);

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::JoinRoomRequest,
			*this,
			&PacketPayloadValidator::ValidateJoinRoomRequestPacket,
			&UdpServer::HandleJoinRoomRequest
		);
	}

	UdpPacketDispatcher::DispatchResult UdpServer::DispatchPacket(const sockaddr_in& remoteAddress, const char* packetData, int packetSize)
	{
		const std::optional<common::packet::PacketHeader> packetHeader = common::packet::DeserializePacketHeader(packetData, packetSize);
		if (packetHeader.has_value() && common::packet::IsReliablePacketHeader(*packetHeader))
		{
			return DispatchReliablePacket(remoteAddress, packetData, packetSize);
		}

		return packetDispatcher_.Dispatch(remoteAddress, packetData, packetSize);
	}

	UdpPacketDispatcher::DispatchResult UdpServer::DispatchReliablePacket(const sockaddr_in& remoteAddress, const char* packetData, int packetSize)
	{
		using DispatchResult = UdpPacketDispatcher::DispatchResult;
		using DispatchStatus = UdpPacketDispatcher::DispatchStatus;

		const std::optional<common::packet::ReliableUdpPacketView> packetView = common::packet::ParseReliableUdpPacket(packetData, packetSize);
		if (!packetView.has_value())
		{
			serverMetricsCollector_.IncrementInvalidReliablePacketCount();
			return DispatchResult{ DispatchStatus::InvalidPacketHeader, std::nullopt, packetSize };
		}

		const EndpointKey endpointKey = common::net::MakeEndpointKey(remoteAddress);
		const bool isAckOnlyPacket = packetView->packetHeader.type == common::packet::PacketType::None;

		bool isNewReliablePacket = false;
		std::optional<common::packet::PacketBuffer> ackPacketBuffer;

		{
			std::scoped_lock lock(stateMutex_);

			PeerState* peerState = peerRoomManager_.FindJoinedPeer(endpointKey);
			if (peerState == nullptr)
			{
				serverMetricsCollector_.IncrementReliableUnknownPeerPacketCount();

				if (isAckOnlyPacket)
				{
					serverMetricsCollector_.IncrementReliableUnknownPeerAckOnlyPacketCount();
				}
				else
				{
					serverMetricsCollector_.IncrementReliableUnknownPeerDataPacketCount();
				}

				return DispatchResult{ DispatchStatus::InvalidPacketHeader, packetView->packetHeader.type, packetSize };
			}

			if (isAckOnlyPacket)
			{
				const bool ackProcessed = peerState->reliableSession.ProcessReceivedAck(packetView->reliableHeader);
				if (!ackProcessed)
				{
					serverMetricsCollector_.IncrementReliableInvalidAckPacketCount();
				}

				serverMetricsCollector_.IncrementReliableAckOnlyReceivePacketCount();
			}
			else
			{
				serverMetricsCollector_.IncrementReliableDataReceivePacketCount();

				isNewReliablePacket = peerState->reliableSession.ProcessReceivedDataHeader(packetView->reliableHeader);
				ackPacketBuffer = BuildReliableAckPacket(*peerState);
			}
		}

		if (isAckOnlyPacket)
		{
			return DispatchResult{ DispatchStatus::Succeeded, packetView->packetHeader.type, packetSize };
		}

		if (ackPacketBuffer.has_value())
		{
			packetSender_.SendPacket(
				remoteAddress,
				ackPacketBuffer->data(),
				static_cast<int>(ackPacketBuffer->size())
			);

			serverMetricsCollector_.IncrementReliableAckOnlySendPacketCount();
		}

		if (!isNewReliablePacket)
		{
			serverMetricsCollector_.IncrementReliableDuplicateDropPacketCount();
			return DispatchResult{ DispatchStatus::Succeeded, packetView->packetHeader.type, packetSize };
		}

		const std::optional<common::packet::PacketBuffer> gamePacketBuffer =
			common::packet::BuildGamePacketFromReliableUdpPacketView(*packetView);
		if (!gamePacketBuffer.has_value())
		{
			return DispatchResult{ DispatchStatus::InvalidPacketPayload, packetView->packetHeader.type, packetSize };
		}

		return packetDispatcher_.Dispatch(remoteAddress, gamePacketBuffer->data(), static_cast<int>(gamePacketBuffer->size()));
	}

	std::optional<common::packet::PacketBuffer> UdpServer::BuildReliableGamePacket(PeerState& peerState, common::packet::ConstPacketSpan serializedGamePacket)
	{
		const std::optional<common::packet::PacketHeader> packetHeader =
			common::packet::DeserializePacketHeader(
				serializedGamePacket.data(),
				static_cast<int>(serializedGamePacket.size())
			);

		if (!packetHeader.has_value())
		{
			return std::nullopt;
		}

		if (common::packet::IsReliablePacketHeader(*packetHeader))
		{
			return std::nullopt;
		}

		if (common::packet::GetPacketHeaderProtocolVersion(*packetHeader) != common::packet::protocolVersion)
		{
			return std::nullopt;
		}

		if (static_cast<std::size_t>(packetHeader->size) != serializedGamePacket.size())
		{
			return std::nullopt;
		}

		if (!common::packet::IsReliablePacketType(packetHeader->type))
		{
			return std::nullopt;
		}

		const common::net::ReliableSequence sequence = peerState.reliableSession.AllocateOutgoingSequence();
		const common::net::ReliableUdpPacketHeader reliableHeader = peerState.reliableSession.BuildOutgoingHeader(sequence);

		const std::optional<common::packet::PacketBuffer> reliablePacketBuffer =
			common::packet::BuildReliableUdpPacket(reliableHeader, serializedGamePacket);

		if (!reliablePacketBuffer.has_value())
		{
			return std::nullopt;
		}

		const common::net::ReliableUdpSession::TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();
		if (!peerState.reliableSession.RegisterSentPacket(sequence, *reliablePacketBuffer, currentTime))
		{
			serverMetricsCollector_.IncrementReliableSendWindowFullCount();
			return std::nullopt;
		}

		serverMetricsCollector_.IncrementReliableDataSendPacketCount();

		return reliablePacketBuffer;
	}

	std::optional<common::packet::PacketBuffer> UdpServer::BuildReliableAckPacket(PeerState& peerState)
	{
		const common::net::ReliableUdpPacketHeader reliableHeader = peerState.reliableSession.BuildOutgoingAckHeader();

		return common::packet::BuildReliableUdpAckPacket(reliableHeader);
	}

	std::optional<common::packet::PacketBuffer> UdpServer::BuildReliableJoinRoomResponse(PeerState& peerState, RoomId roomId, float spawnX, float spawnY)
	{
		common::packet::JoinRoomResponsePacket packet{};
		packet.roomId = roomId;
		packet.spawnX = spawnX;
		packet.spawnY = spawnY;

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return std::nullopt;
		}

		return BuildReliableGamePacket(peerState, common::packet::ConstPacketSpan(packetBuffer->data(), packetBuffer->size()));
	}

	void UdpServer::HandleJoinRequest(const sockaddr_in& remoteAddress)
	{
		serverMetricsCollector_.IncrementJoinRequestCount();
		ProcessJoinRequest(remoteAddress);
	}

	void UdpServer::HandleInputCommand(const sockaddr_in& remoteAddress, const common::packet::InputCommandPacket& packet)
	{
		serverMetricsCollector_.IncrementInputCommandCount();

		const EndpointKey endpointKey = common::net::MakeEndpointKey(remoteAddress);
		ProcessInputCommand(endpointKey, packet);
	}

	void UdpServer::HandleFireRequest(const sockaddr_in& remoteAddress)
	{
		serverMetricsCollector_.IncrementFireRequestCount();

		const EndpointKey endpointKey = common::net::MakeEndpointKey(remoteAddress);
		ProcessFireRequest(endpointKey);
	}

	void UdpServer::HandleLeaveRequest(const sockaddr_in& remoteAddress)
	{
		serverMetricsCollector_.IncrementLeaveRequestCount();

		const EndpointKey endpointKey = common::net::MakeEndpointKey(remoteAddress);
		ProcessLeaveRequest(endpointKey);
	}

	void UdpServer::HandleJoinRoomRequest(const sockaddr_in& remoteAddress, const common::packet::JoinRoomRequestPacket& packet)
	{
		serverMetricsCollector_.IncrementJoinRoomRequestCount();

		const EndpointKey endpointKey = common::net::MakeEndpointKey(remoteAddress);
		ProcessJoinRoomRequest(endpointKey, packet);
	}

	void UdpServer::ProcessReliableResends()
	{
		struct ReliableResendTask
		{
		public:
			sockaddr_in remoteAddress{};
			common::packet::PacketBuffer packetBuffer;
		};

		std::vector<ReliableResendTask> resendTaskList;
		std::uint64_t giveUpPacketCount = 0;

		{
			std::scoped_lock lock(stateMutex_);

			const common::net::ReliableUdpSession::TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

			peerRoomManager_.ForEachJoinedPeer(
				[&resendTaskList, &giveUpPacketCount, currentTime](PeerState& peerState)
				{
					common::net::ReliableUdpSession::ResendResult resendResult = peerState.reliableSession.ExtractResendResult(currentTime);

					giveUpPacketCount += static_cast<std::uint64_t>(resendResult.giveUpPacketList.size());

					for (common::net::ReliablePendingPacket& pendingPacket : resendResult.resendPacketList)
					{
						ReliableResendTask resendTask{};
						resendTask.remoteAddress = peerState.remoteAddress;
						resendTask.packetBuffer = std::move(pendingPacket.packetBuffer);

						resendTaskList.push_back(std::move(resendTask));
					}
				}
			);
		}

		for (const ReliableResendTask& resendTask : resendTaskList)
		{
			packetSender_.SendPacket(
				resendTask.remoteAddress,
				resendTask.packetBuffer.data(),
				static_cast<int>(resendTask.packetBuffer.size())
			);
		}

		serverMetricsCollector_.AddReliableResendPacketCount(static_cast<std::uint64_t>(resendTaskList.size()));
		serverMetricsCollector_.AddReliableResendGiveUpPacketCount(giveUpPacketCount);

		if (giveUpPacketCount > 0)
		{
			std::ostringstream stream;
			stream << "Reliable resend give-up packets detected. Count=" << giveUpPacketCount;
			LogWarning(stream.str());
		}
	}

	void UdpServer::ProcessJoinRequest(const sockaddr_in& remoteAddress)
	{
		const EndpointKey endpointKey = common::net::MakeEndpointKey(remoteAddress);

		PeerSessionService::JoinResult joinResult{};

		{
			std::scoped_lock lock(stateMutex_);

			joinResult = peerSessionService_.JoinPeer(
				remoteAddress,
				endpointKey,
				config_.session.initialRoomId,
				peerRoomManager_,
				gameWorld_,
				gameSimulation_,
				config_.gameRule,
				config_.reliableUdp,
				std::chrono::steady_clock::now()
			);
		}

		if (!joinResult.shouldSendResponse)
		{
			LogWarning("Join request ignored.");
			return;
		}

		packetSender_.SendJoinResponse(
			joinResult.remoteAddress,
			joinResult.playerId,
			joinResult.spawnPosition.x,
			joinResult.spawnPosition.y
		);

		if (joinResult.shouldBroadcastPlayerJoined)
		{
			{
				std::ostringstream stream;
				stream << "Peer joined. Endpoint=" << FormatEndpoint(joinResult.remoteAddress)
					<< ", PlayerId=" << joinResult.playerId
					<< ", RoomId=" << joinResult.roomId;
				LogInfo(stream.str());
			}

			BroadcastPlayerJoined(
				joinResult.roomId,
				joinResult.playerId,
				joinResult.spawnPosition.x,
				joinResult.spawnPosition.y
			);
		}
		else
		{
			std::ostringstream stream;
			stream << "Join response sent to existing peer. Endpoint=" << FormatEndpoint(joinResult.remoteAddress)
				<< ", PlayerId=" << joinResult.playerId
				<< ", RoomId=" << joinResult.roomId;
			LogDebug(stream.str());
		}
	}

	void UdpServer::ProcessInputCommand(const EndpointKey& endpointKey, const common::packet::InputCommandPacket& packet)
	{
		std::scoped_lock lock(stateMutex_);

		if (!playerCommandService_.ApplyInputCommand(
			endpointKey,
			packet,
			peerRoomManager_,
			gameWorld_,
			std::chrono::steady_clock::now()))
		{
			return;
		}
	}

	void UdpServer::ProcessFireRequest(const EndpointKey& endpointKey)
	{
		std::scoped_lock lock(stateMutex_);

		if (!playerCommandService_.FireBullet(
			endpointKey,
			peerRoomManager_,
			gameWorld_,
			gameSimulation_,
			config_.weaponRule,
			std::chrono::steady_clock::now()))
		{
			return;
		}
	}

	void UdpServer::ProcessLeaveRequest(const EndpointKey& endpointKey)
	{
		PeerSessionService::LeaveResult leaveResult{};

		{
			std::scoped_lock lock(stateMutex_);

			leaveResult = peerSessionService_.LeavePeer(
				endpointKey,
				peerRoomManager_,
				gameWorld_
			);
		}

		if (!leaveResult.shouldBroadcastPlayerLeft)
		{
			LogDebug("Leave request ignored because peer was not joined.");
			return;
		}

		{
			std::ostringstream stream;
			stream << "Peer left. PlayerId=" << leaveResult.playerId
				<< ", RoomId=" << leaveResult.roomId;
			LogInfo(stream.str());
		}

		BroadcastPlayerLeft(leaveResult.roomId, leaveResult.playerId);
	}

	void UdpServer::ProcessJoinRoomRequest(const EndpointKey& endpointKey, const common::packet::JoinRoomRequestPacket& packet)
	{
		PeerSessionService::RoomChangeResult roomChangeResult{};
		std::optional<common::packet::PacketBuffer> reliableResponsePacketBuffer;

		{
			std::scoped_lock lock(stateMutex_);

			roomChangeResult = peerSessionService_.ChangePeerRoom(
				endpointKey,
				packet.roomId,
				peerRoomManager_,
				gameWorld_,
				gameSimulation_,
				std::chrono::steady_clock::now()
			);

			if (roomChangeResult.changed)
			{
				PeerState* peerState = peerRoomManager_.FindJoinedPeer(endpointKey);
				if (peerState != nullptr)
				{
					reliableResponsePacketBuffer = BuildReliableJoinRoomResponse(
						*peerState,
						roomChangeResult.nextRoomId,
						roomChangeResult.spawnPosition.x,
						roomChangeResult.spawnPosition.y
					);
				}
			}
		}

		if (!roomChangeResult.changed)
		{
			LogDebug("Join room request ignored.");
			return;
		}

		if (reliableResponsePacketBuffer.has_value())
		{
			packetSender_.SendPacket(
				roomChangeResult.remoteAddress,
				reliableResponsePacketBuffer->data(),
				static_cast<int>(reliableResponsePacketBuffer->size())
			);
		}
		else
		{
			packetSender_.SendJoinRoomResponse(
				roomChangeResult.remoteAddress,
				roomChangeResult.nextRoomId,
				roomChangeResult.spawnPosition.x,
				roomChangeResult.spawnPosition.y
			);
		}

		{
			std::ostringstream stream;
			stream << "Peer changed room. Endpoint=" << FormatEndpoint(roomChangeResult.remoteAddress)
				<< ", PlayerId=" << roomChangeResult.playerId
				<< ", PreviousRoomId=" << roomChangeResult.previousRoomId
				<< ", NextRoomId=" << roomChangeResult.nextRoomId;
			LogInfo(stream.str());
		}

		BroadcastPlayerLeft(
			roomChangeResult.previousRoomId,
			roomChangeResult.playerId
		);

		BroadcastPlayerJoined(
			roomChangeResult.nextRoomId,
			roomChangeResult.playerId,
			roomChangeResult.spawnPosition.x,
			roomChangeResult.spawnPosition.y
		);
	}

	void UdpServer::BroadcastPlayerSnapshots()
	{
		std::vector<PlayerSnapshotTask> playerSnapshotTaskList;

		{
			std::scoped_lock lock(stateMutex_);

			playerSnapshotTaskList = snapshotBroadcastBuilder_.BuildPlayerSnapshotTasks(
				peerRoomManager_.GetRoomTable(),
				peerRoomManager_.GetPeerTable(),
				gameWorld_
			);
		}

		const std::size_t sentCount = packetSender_.SendPlayerSnapshotTasks(playerSnapshotTaskList);
		serverMetricsCollector_.AddPlayerSnapshotSendRequestCount(static_cast<std::uint64_t>(sentCount));
	}

	void UdpServer::BroadcastBulletSnapshots()
	{
		std::vector<BulletSnapshotTask> bulletSnapshotTaskList;

		{
			std::scoped_lock lock(stateMutex_);

			bulletSnapshotTaskList = snapshotBroadcastBuilder_.BuildBulletSnapshotTasks(
				peerRoomManager_.GetRoomTable(),
				peerRoomManager_.GetPeerTable(),
				gameWorld_
			);
		}

		const std::size_t sentCount = packetSender_.SendBulletSnapshotTasks(bulletSnapshotTaskList);
		serverMetricsCollector_.AddBulletSnapshotSendRequestCount(static_cast<std::uint64_t>(sentCount));
	}

	void UdpServer::BroadcastImpactEffects()
	{
		std::vector<ImpactEffectTask> impactEffectTaskList;

		{
			std::scoped_lock lock(stateMutex_);

			if (!gameWorld_.HasPendingImpactEffects())
			{
				return;
			}

			impactEffectTaskList = snapshotBroadcastBuilder_.BuildImpactEffectTasks(
				peerRoomManager_.GetRoomTable(),
				peerRoomManager_.GetPeerTable(),
				gameWorld_
			);

			gameWorld_.ClearPendingImpactEffects();
		}

		const std::size_t sentCount = packetSender_.SendImpactEffectTasks(impactEffectTaskList);
		serverMetricsCollector_.AddImpactEffectSendRequestCount(static_cast<std::uint64_t>(sentCount));
	}

	void UdpServer::BroadcastPlayerJoined(RoomId roomId, PlayerId playerId, float x, float y)
	{
		std::vector<sockaddr_in> remoteAddressList = [&]()
			{
				std::scoped_lock lock(stateMutex_);
				return peerRoomManager_.BuildRoomRemoteAddressList(roomId);
			}();

		packetSender_.BroadcastPlayerJoined(remoteAddressList, roomId, playerId, x, y);
	}

	void UdpServer::BroadcastPlayerLeft(RoomId roomId, PlayerId playerId)
	{
		std::vector<sockaddr_in> remoteAddressList = [&]()
			{
				std::scoped_lock lock(stateMutex_);
				return peerRoomManager_.BuildRoomRemoteAddressList(roomId);
			}();

		packetSender_.BroadcastPlayerLeft(remoteAddressList, roomId, playerId);
	}

	void UdpServer::RemoveTimedOutPeers()
	{
		struct TimedOutBroadcast
		{
		public:
			PlayerId playerId = 0;
			RoomId roomId = 0;
		};

		std::vector<TimedOutBroadcast> timedOutBroadcastList;

		{
			std::scoped_lock lock(stateMutex_);

			const std::vector<PeerRoomManager::TimedOutPeer> timedOutPeerList = peerRoomManager_.RemoveTimedOutPeers(
				std::chrono::steady_clock::now(),
				config_.session.peerTimeout
			);

			serverMetricsCollector_.AddTimedOutPeerCount(timedOutPeerList.size());

			timedOutBroadcastList.reserve(timedOutPeerList.size());

			for (const PeerRoomManager::TimedOutPeer& timedOutPeer : timedOutPeerList)
			{
				gameWorld_.RemovePlayer(timedOutPeer.playerId);

				TimedOutBroadcast timedOutBroadcast{};
				timedOutBroadcast.playerId = timedOutPeer.playerId;
				timedOutBroadcast.roomId = timedOutPeer.roomId;
				timedOutBroadcastList.push_back(timedOutBroadcast);
			}
		}

		for (const TimedOutBroadcast& timedOutBroadcast : timedOutBroadcastList)
		{
			{
				std::ostringstream stream;
				stream << "Peer timed out. PlayerId=" << timedOutBroadcast.playerId
					<< ", RoomId=" << timedOutBroadcast.roomId;
				LogInfo(stream.str());
			}

			BroadcastPlayerLeft(timedOutBroadcast.roomId, timedOutBroadcast.playerId);
		}
	}

	void UdpServer::LogDebug(std::string_view message) const
	{
		if (logger_ == nullptr)
		{
			return;
		}

		logger_->Debug(message);
	}

	void UdpServer::LogInfo(std::string_view message) const
	{
		if (logger_ == nullptr)
		{
			return;
		}

		logger_->Info(message);
	}

	void UdpServer::LogWarning(std::string_view message) const
	{
		if (logger_ == nullptr)
		{
			return;
		}

		logger_->Warning(message);
	}

	void UdpServer::LogError(std::string_view message) const
	{
		if (logger_ == nullptr)
		{
			return;
		}

		logger_->Error(message);
	}

	void UdpServer::LogInvalidPacket(const sockaddr_in& remoteAddress, const UdpPacketDispatcher::DispatchResult& dispatchResult)
	{
		const InvalidPacketLogLimiter::LogDecision logDecision = invalidPacketLogLimiter_.Record(
			dispatchResult.status,
			InvalidPacketLogLimiter::Clock::now()
		);

		if (!logDecision.shouldLog)
		{
			return;
		}

		std::ostringstream stream;
		stream << "Invalid UDP packet dropped. "
			<< "Endpoint=" << FormatEndpoint(remoteAddress)
			<< ", Reason=" << UdpPacketDispatcher::ToString(dispatchResult.status)
			<< ", ActualSize=" << dispatchResult.actualPacketSize;

		if (dispatchResult.declaredPacketSize > 0)
		{
			stream << ", DeclaredSize=" << dispatchResult.declaredPacketSize;
		}

		if (dispatchResult.expectedPacketSize > 0)
		{
			stream << ", ExpectedSize=" << dispatchResult.expectedPacketSize;
		}

		if (dispatchResult.packetType.has_value())
		{
			stream << ", PacketType=" << static_cast<int>(*dispatchResult.packetType);
		}

		if (dispatchResult.protocolVersion != 0)
		{
			stream << ", ProtocolVersion=" << dispatchResult.protocolVersion;
		}

		stream << ", TotalDroppedForReason=" << logDecision.totalCount;

		if (logDecision.suppressedCount > 0)
		{
			stream << ", SuppressedSinceLastLog=" << logDecision.suppressedCount;
		}

		if (dispatchResult.status == UdpPacketDispatcher::DispatchStatus::InvalidPacketPayload
			&& dispatchResult.detailCode != 0)
		{
			const auto payloadValidationStatus = static_cast<PacketPayloadValidator::PayloadValidationStatus>(dispatchResult.detailCode);

			stream << ", PayloadReason=" << PacketPayloadValidator::ToString(payloadValidationStatus);
		}

		LogWarning(stream.str());
	}

	void UdpServer::LogServerStatusIfDue()
	{
		if (!serverStatusReporter_.ShouldReport(diagnostics::ServerStatusReporter::Clock::now()))
		{
			return;
		}

		const diagnostics::ServerStatusSnapshot snapshot = BuildServerStatusSnapshot();
		LogInfo(serverStatusReporter_.BuildMessage(snapshot));
	}

	diagnostics::ServerStatusSnapshot UdpServer::BuildServerStatusSnapshot() const
	{
		diagnostics::ServerStatusSnapshot snapshot{};

		{
			std::scoped_lock lock(stateMutex_);

			snapshot.serverTick = gameWorld_.GetServerTick();

			snapshot.peerCount = peerRoomManager_.GetPeerCount();
			snapshot.joinedPeerCount = peerRoomManager_.GetJoinedPeerCount();
			snapshot.roomCount = peerRoomManager_.GetRoomCount();

			snapshot.playerCount = gameWorld_.GetPlayerCount();
			snapshot.bulletCount = gameWorld_.GetBulletCount();
			snapshot.pendingImpactEffectCount = gameWorld_.GetPendingImpactEffectCount();

			std::size_t reliablePendingPacketCount = 0;
			peerRoomManager_.ForEachJoinedPeer(
				[&reliablePendingPacketCount](const PeerState& peerState)
				{
					reliablePendingPacketCount += peerState.reliableSession.GetPendingPacketCount();
				}
			);

			snapshot.reliablePendingPacketCount = reliablePendingPacketCount;
		}

		const net::UdpIocpTransportMetricsSnapshot transportMetrics = udpTransport_.CaptureMetricsSnapshot();

		snapshot.pendingSendContextCount = udpTransport_.GetPendingSendContextCount();
		snapshot.udpSendCompletionCount = transportMetrics.sendCompletionCount;
		snapshot.udpSendCompletionFailureCount = transportMetrics.sendCompletionFailureCount;
		snapshot.udpSendCompletedByteCount = transportMetrics.sendCompletedByteCount;

		snapshot.metrics = serverMetricsCollector_.CaptureSnapshot();

		return snapshot;
	}
}