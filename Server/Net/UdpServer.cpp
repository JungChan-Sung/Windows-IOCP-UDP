#include "UdpServer.h"

#include <WS2tcpip.h>

#include <array>
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
#include <Common/Net/Reliable/ReliableUdpPacketBuilder.h>
#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketReliability.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/String/StringFormat.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Config/ServerConfigValidator.h>
#include <Server/Protocol/AccountLoginPacketHandler.h>
#include <Server/Protocol/AccountPacketMapper.h>
#include <Server/Protocol/PacketPayloadValidator.h>

namespace
{
	template <typename TObject>
	using AddressOnlyPacketHandler = void (TObject::*)(const sockaddr_in&);

	template <typename TObject, typename TPacket>
	using AddressTypedPacketHandler = void (TObject::*)(const sockaddr_in&, const TPacket&);

	template <typename TObject>
	void RegisterAddressOnlyPacketHandler(
		server::protocol::UdpPacketDispatcher& packetDispatcher,
		common::packet::PacketType packetType,
		TObject& object,
		int expectedPacketSize,
		AddressOnlyPacketHandler<TObject> handler
	)
	{
		using DispatchStatus = server::protocol::UdpPacketDispatcher::DispatchStatus;
		using PacketProcessResult = server::protocol::UdpPacketDispatcher::PacketProcessResult;

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

	template <typename TObject, typename TPacket>
	void RegisterTypedPacketHandler(
		server::protocol::UdpPacketDispatcher& packetDispatcher,
		common::packet::PacketType packetType,
		TObject& object,
		AddressTypedPacketHandler<TObject, TPacket> handler
	)
	{
		using Packet = std::remove_cvref_t<TPacket>;
		using DispatchStatus = server::protocol::UdpPacketDispatcher::DispatchStatus;
		using PacketProcessResult = server::protocol::UdpPacketDispatcher::PacketProcessResult;

		packetDispatcher.RegisterHandler(
			packetType,
			common::packet::packetExpectedSize<Packet>,
			[&object, handler](const sockaddr_in& remoteAddress, const char* packetData, int packetSize)
			{
				std::optional<Packet> packet = common::packet::DeserializePacket<Packet>(packetData, packetSize);
				if (!packet.has_value())
				{
					return PacketProcessResult{ DispatchStatus::InvalidPacketPayload, 0 };
				}

				std::invoke(handler, object, remoteAddress, *packet);

				return PacketProcessResult{ DispatchStatus::Succeeded, 0 };
			}
		);
	}

	template <typename TObject, typename TPacket, typename TValidator>
	void RegisterTypedPacketHandler(
		server::protocol::UdpPacketDispatcher& packetDispatcher,
		common::packet::PacketType packetType,
		TObject& object,
		TValidator validator,
		AddressTypedPacketHandler<TObject, TPacket> handler
	)
	{
		using Packet = std::remove_cvref_t<TPacket>;
		using DispatchStatus = server::protocol::UdpPacketDispatcher::DispatchStatus;
		using PacketProcessResult = server::protocol::UdpPacketDispatcher::PacketProcessResult;
		using PayloadValidationStatus = server::protocol::PacketPayloadValidator::PayloadValidationStatus;

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

	UdpServer::StartResult UdpServer::Start(const config::ServerConfig& config)
	{
		if (isRunning_.load())
		{
			LogWarning("UdpServer start ignored because server is already running.");
			return std::unexpected(StartError{ StartFailure::AlreadyRunning });
		}

		config_ = config;

		const std::vector<config::ServerConfigWarning> warningList
			= config::ServerConfigValidator::ValidateAndNormalize(config_);

		for (const config::ServerConfigWarning& warning : warningList)
		{
			LogWarning(warning.message);
		}

		packetSender_.SetFaultSimulationConfig(config_.udpFaultSimulation);

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

				const protocol::UdpPacketDispatcher::DispatchResult dispatchResult = DispatchPacket(
					remoteAddress,
					packetData,
					packetSize
				);

				if (dispatchResult.status != protocol::UdpPacketDispatcher::DispatchStatus::Succeeded)
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
		config::ServerConfig config{};
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
		packetSender_.ResetFaultSimulation();
		packetDispatcher_.Clear();

		{
			std::scoped_lock lock(stateMutex_);

			if (wasRunning)
			{
				matchHistoryTracker_.CompleteAll(common::time::SystemClock::now());
			}

			authenticatedAccountRegistry_.Clear();
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

	void UdpServer::AttachAccountLoginPacketHandler(protocol::AccountLoginPacketHandler& accountLoginPacketHandler) noexcept
	{
		accountLoginPacketHandler_ = &accountLoginPacketHandler;
	}

	void UdpServer::DetachAccountLoginPacketHandler() noexcept
	{
		accountLoginPacketHandler_ = nullptr;
	}

	game::CompletedMatchList UdpServer::ExtractCompletedMatches()
	{
		std::scoped_lock lock(stateMutex_);
		return matchHistoryTracker_.ExtractCompletedMatches();
	}

	void UdpServer::UpdateGameTick()
	{
		if (!isRunning_.load())
		{
			return;
		}

		ProcessAccountLoginResponses();

		std::size_t killEventCount = 0;
		std::size_t recordedKillCount = 0;

		{
			std::scoped_lock lock(stateMutex_);

			const game::PlayerSimulationContextList playerContextList = BuildPlayerSimulationContextList();

			gameSimulation_.UpdatePlayers(config_.tick.fixedDeltaSeconds, playerContextList, gameWorld_);
			const game::KillEventList killEventList = gameSimulation_.UpdateBullets(
				config_.tick.fixedDeltaSeconds,
				playerContextList,
				gameWorld_,
				config_.gameRule
			);

			killEventCount = killEventList.size();
			recordedKillCount = matchHistoryTracker_.RecordKills(killEventList);

			gameSimulation_.UpdateRespawns(config_.tick.fixedDeltaSeconds, playerContextList, gameWorld_, config_.gameRule);
			gameSimulation_.UpdatePlayerTimers(config_.tick.fixedDeltaSeconds, gameWorld_);

			gameWorld_.AdvanceServerTick();
		}

		if (recordedKillCount != killEventCount)
		{
			LogError("Some kill events could not be recorded in match history.");
		}

		RemoveTimedOutPeers();
		ProcessReliableResends();

		const std::size_t faultSimulationReleasedSendRequestCount = packetSender_.FlushFaultSimulationPackets();
		serverMetricsCollector_.AddFaultSimulationReleasedSendRequestCount(static_cast<std::uint64_t>(faultSimulationReleasedSendRequestCount));

		BroadcastPlayerSnapshots();
		BroadcastBulletSnapshots();
		BroadcastImpactEffects();

		LogServerStatusIfDue();
	}

	game::PlayerSimulationContextList UdpServer::BuildPlayerSimulationContextList() const
	{
		game::PlayerSimulationContextList playerContextList;
		playerContextList.reserve(peerRoomManager_.GetJoinedPeerCount());

		peerRoomManager_.ForEachJoinedPeer(
			[&playerContextList](const service::PeerState& peerState)
			{
				playerContextList.push_back(game::PlayerSimulationContext{
					.playerId = peerState.playerId,
					.persistentPlayerId = peerState.persistentPlayerId,
					.roomId = peerState.roomId,
					});
			}
		);

		return playerContextList;
	}

	void UdpServer::RegisterPacketHandlers()
	{
		packetDispatcher_.Clear();

		if (accountLoginPacketHandler_ != nullptr)
		{
			RegisterTypedPacketHandler(
				packetDispatcher_,
				common::packet::PacketType::AccountLoginRequest,
				*this,
				&UdpServer::HandleAccountLoginRequest
			);
		}

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::JoinRequest,
			*this,
			&UdpServer::HandleJoinRequest
		);

		RegisterTypedPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::InputCommand,
			*this,
			&protocol::PacketPayloadValidator::ValidateInputCommandPacket,
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
			&protocol::PacketPayloadValidator::ValidateJoinRoomRequestPacket,
			&UdpServer::HandleJoinRoomRequest
		);
	}

	protocol::UdpPacketDispatcher::DispatchResult UdpServer::DispatchPacket(const sockaddr_in& remoteAddress, const char* packetData, int packetSize)
	{
		const std::optional<common::packet::PacketHeader> packetHeader = common::packet::DeserializePacketHeader(packetData, packetSize);
		if (packetHeader.has_value() && common::packet::IsReliablePacketHeader(*packetHeader))
		{
			return DispatchReliablePacket(remoteAddress, packetData, packetSize);
		}

		return packetDispatcher_.Dispatch(remoteAddress, packetData, packetSize);
	}

	protocol::UdpPacketDispatcher::DispatchResult UdpServer::DispatchReliablePacket(const sockaddr_in& remoteAddress, const char* packetData, int packetSize)
	{
		using DispatchResult = protocol::UdpPacketDispatcher::DispatchResult;
		using DispatchStatus = protocol::UdpPacketDispatcher::DispatchStatus;

		const std::optional<common::net::ReliableUdpPacketView> packetView = common::net::ParseReliableUdpPacket(packetData, packetSize);
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

			service::PeerState* peerState = peerRoomManager_.FindJoinedPeer(endpointKey);
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
			common::net::BuildGamePacketFromReliableUdpPacketView(*packetView);
		if (!gamePacketBuffer.has_value())
		{
			return DispatchResult{ DispatchStatus::InvalidPacketPayload, packetView->packetHeader.type, packetSize };
		}

		return packetDispatcher_.Dispatch(remoteAddress, gamePacketBuffer->data(), static_cast<int>(gamePacketBuffer->size()));
	}

	std::optional<common::packet::PacketBuffer> UdpServer::BuildReliableGamePacket(service::PeerState& peerState, common::packet::ConstPacketSpan serializedGamePacket)
	{
		const std::optional<common::packet::PacketHeader> packetHeader = common::packet::DeserializePacketHeader(
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
			common::net::BuildReliableUdpPacket(reliableHeader, serializedGamePacket);

		if (!reliablePacketBuffer.has_value())
		{
			return std::nullopt;
		}

		const common::net::ReliableUdpSession::TimePoint currentTime = common::time::Clock::now();
		if (!peerState.reliableSession.RegisterSentPacket(sequence, *reliablePacketBuffer, currentTime))
		{
			serverMetricsCollector_.IncrementReliableSendWindowFullCount();
			return std::nullopt;
		}

		serverMetricsCollector_.IncrementReliableDataSendPacketCount();

		return reliablePacketBuffer;
	}

	std::optional<common::packet::PacketBuffer> UdpServer::BuildReliableAckPacket(service::PeerState& peerState)
	{
		const common::net::ReliableUdpPacketHeader reliableHeader = peerState.reliableSession.BuildOutgoingAckHeader();

		return common::net::BuildReliableUdpAckPacket(reliableHeader);
	}

	std::optional<common::packet::PacketBuffer> UdpServer::BuildReliableJoinRoomResponse(service::PeerState& peerState, RoomId roomId, float spawnX, float spawnY)
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

	void UdpServer::HandleJoinRequest(const sockaddr_in& remoteAddress, const common::packet::JoinRequestPacket& packet)
	{
		serverMetricsCollector_.IncrementJoinRequestCount();
		ProcessJoinRequest(remoteAddress, packet);
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

	void UdpServer::HandleAccountLoginRequest(const sockaddr_in& remoteAddress, const common::packet::AccountLoginRequestPacket& packet)
	{
		if (accountLoginPacketHandler_ == nullptr)
		{
			return;
		}

		const protocol::AccountLoginPacketHandler::EnqueueStatus enqueueStatus = accountLoginPacketHandler_->Enqueue(
			remoteAddress,
			packet,
			common::time::Clock::now()
		);

		switch (enqueueStatus)
		{
		case protocol::AccountLoginPacketHandler::EnqueueStatus::Enqueued:
		case protocol::AccountLoginPacketHandler::EnqueueStatus::DuplicatePending:
		case protocol::AccountLoginPacketHandler::EnqueueStatus::CachedResponseQueued:
			return;

		case protocol::AccountLoginPacketHandler::EnqueueStatus::TaskEnqueueFailed:
		default:
			break;
		}

		common::packet::AccountLoginResponsePacket responsePacket{};
		responsePacket.requestId = packet.requestId;
		responsePacket.status = common::packet::AccountLoginResponseStatus::ServerError;
		if (!packetSender_.SendAccountLoginResponse(remoteAddress, responsePacket))
		{
			LogWarning("Failed to send account login server error response.");
		}
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

			const common::net::ReliableUdpSession::TimePoint currentTime = common::time::Clock::now();

			peerRoomManager_.ForEachJoinedPeer(
				[&resendTaskList, &giveUpPacketCount, currentTime](service::PeerState& peerState)
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

	void UdpServer::ProcessJoinRequest(const sockaddr_in& remoteAddress, const common::packet::JoinRequestPacket& packet)
	{
		const EndpointKey endpointKey = common::net::MakeEndpointKey(remoteAddress);

		service::PeerSessionService::JoinResult joinResult{};
		bool hasAuthenticatedIdentity = false;
		bool matchHistoryEntered = true;

		{
			std::scoped_lock lock(stateMutex_);

			service::PeerSessionService::AuthenticatedIdentity authenticatedIdentity{};

			const service::PeerState* existingPeerState = peerRoomManager_.FindJoinedPeer(endpointKey);
			if (existingPeerState != nullptr)
			{
				// JoinResponse 유실로 인한 기존 참가자의 재요청.
				// 패킷 토큰은 JoinPeer()에서 PeerState 토큰과 비교한다.
				authenticatedIdentity.accountId = existingPeerState->accountId;
				authenticatedIdentity.persistentPlayerId = existingPeerState->persistentPlayerId;
				authenticatedIdentity.sessionToken = packet.sessionToken;
				authenticatedIdentity.nickname = existingPeerState->nickname;

				hasAuthenticatedIdentity = true;
			}
			else
			{
				const service::AuthenticatedAccount* authenticatedAccount = authenticatedAccountRegistry_.Find(endpointKey, packet.sessionToken);
				if (authenticatedAccount != nullptr)
				{
					authenticatedIdentity.accountId = authenticatedAccount->accountId;
					authenticatedIdentity.persistentPlayerId = authenticatedAccount->persistentPlayerId;
					authenticatedIdentity.sessionToken = authenticatedAccount->sessionToken;
					authenticatedIdentity.nickname = authenticatedAccount->nickname;

					hasAuthenticatedIdentity = true;
				}
			}

			if (hasAuthenticatedIdentity)
			{
				joinResult = peerSessionService_.JoinPeer(
					remoteAddress,
					endpointKey,
					authenticatedIdentity,
					config_.session.initialRoomId,
					peerRoomManager_,
					gameWorld_,
					config_.gameRule,
					config_.reliableUdp,
					common::time::Clock::now()
				);

				if (joinResult.shouldBroadcastPlayerJoined)
				{
					matchHistoryEntered = matchHistoryTracker_.EnterPlayer(
						joinResult.roomId,
						joinResult.persistentPlayerId,
						common::time::SystemClock::now()
					);

					static_cast<void>(authenticatedAccountRegistry_.Remove(endpointKey));
				}
			}
		}

		if (!matchHistoryEntered)
		{
			LogError("Failed to enter player into match history.");
		}

		if (!hasAuthenticatedIdentity)
		{
			std::ostringstream stream;
			stream << "Unauthenticated join request ignored. Endpoint="
				<< FormatEndpoint(remoteAddress);

			LogWarning(stream.str());
			return;
		}

		if (!joinResult.shouldSendResponse)
		{
			std::ostringstream stream;
			stream << "Join request rejected because the session identity did not match. Endpoint="
				<< FormatEndpoint(remoteAddress);

			LogWarning(stream.str());
			return;
		}

		const bool responseSent = packetSender_.SendJoinResponse(
			joinResult.remoteAddress,
			joinResult.playerId,
			joinResult.roomId,
			joinResult.spawnPosition.x,
			joinResult.spawnPosition.y
		);
		if (!responseSent)
		{
			std::ostringstream stream;
			stream << "Join response send failed. Endpoint=" << FormatEndpoint(joinResult.remoteAddress)
				<< ", PlayerId=" << joinResult.playerId
				<< ", RoomId=" << joinResult.roomId;

			LogWarning(stream.str());
		}

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

			return;
		}

		if (responseSent)
		{
			std::ostringstream stream;
			stream << "Join response sent to existing peer. Endpoint="
				<< FormatEndpoint(joinResult.remoteAddress)
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
			packet.inputSequence,
			packet.inputFlags,
			peerRoomManager_,
			gameWorld_,
			common::time::Clock::now()))
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
			config_.weaponRule,
			common::time::Clock::now()))
		{
			return;
		}
	}

	void UdpServer::ProcessLeaveRequest(const EndpointKey& endpointKey)
	{
		service::PeerSessionService::LeaveResult leaveResult{};
		bool matchHistoryLeft = true;

		{
			std::scoped_lock lock(stateMutex_);

			leaveResult = peerSessionService_.LeavePeer(endpointKey, peerRoomManager_, gameWorld_);
			if (leaveResult.shouldBroadcastPlayerLeft)
			{
				matchHistoryLeft = matchHistoryTracker_.LeavePlayer(
					leaveResult.roomId,
					leaveResult.persistentPlayerId,
					common::time::SystemClock::now()
				);

				if (peerRoomManager_.GetRoomMemberCount(leaveResult.roomId) == 0)
				{
					gameWorld_.ClearRoomTransientState(leaveResult.roomId);
				}
			}
		}

		if (!matchHistoryLeft)
		{
			LogError("Failed to leave player from match history.");
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
		service::PeerSessionService::RoomChangeResult roomChangeResult{};
		std::optional<common::packet::PacketBuffer> reliableResponsePacketBuffer;

		bool previousMatchLeft = true;
		bool nextMatchEntered = true;

		{
			std::scoped_lock lock(stateMutex_);

			roomChangeResult = peerSessionService_.ChangePeerRoom(
				endpointKey,
				packet.roomId,
				peerRoomManager_,
				gameWorld_,
				common::time::Clock::now()
			);

			if (roomChangeResult.changed)
			{
				const common::time::SystemTimePoint currentSystemTime = common::time::SystemClock::now();

				previousMatchLeft = matchHistoryTracker_.LeavePlayer(
					roomChangeResult.previousRoomId,
					roomChangeResult.persistentPlayerId,
					currentSystemTime
				);

				if (peerRoomManager_.GetRoomMemberCount(roomChangeResult.previousRoomId) == 0)
				{
					gameWorld_.ClearRoomTransientState(roomChangeResult.previousRoomId);
				}

				nextMatchEntered = matchHistoryTracker_.EnterPlayer(
					roomChangeResult.nextRoomId,
					roomChangeResult.persistentPlayerId,
					currentSystemTime
				);

				service::PeerState* peerState = peerRoomManager_.FindJoinedPeer(endpointKey);
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

		if (!previousMatchLeft)
		{
			std::ostringstream stream;
			stream << "Failed to leave player from previous match history."
				<< " PersistentPlayerId=" << roomChangeResult.persistentPlayerId
				<< ", RoomId=" << roomChangeResult.previousRoomId;

			LogError(stream.str());
		}

		if (!nextMatchEntered)
		{
			std::ostringstream stream;
			stream << "Failed to enter player into next match history."
				<< " PersistentPlayerId=" << roomChangeResult.persistentPlayerId
				<< ", RoomId=" << roomChangeResult.nextRoomId;

			LogError(stream.str());
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

	void UdpServer::ProcessAccountLoginResponses()
	{
		if (accountLoginPacketHandler_ == nullptr)
		{
			return;
		}

		const common::time::TimePoint currentTime = common::time::Clock::now();
		protocol::AccountLoginPacketHandler::ResponseTaskList responseTaskList = accountLoginPacketHandler_->ExtractResponseTaskList(currentTime);
		for (protocol::AccountLoginPacketHandler::ResponseTask&
			responseTask : responseTaskList)
		{
			if (responseTask.taskId != protocol::AccountLoginPacketHandler::invalidTaskId)
			{
				if (responseTask.isLatestRequest)
				{
					if (responseTask.responsePacket.status == common::packet::AccountLoginResponseStatus::Succeeded)
					{
						const service::AccountLoginAdmissionService::Request admissionRequest{
							.endpointKey = common::net::MakeEndpointKey(responseTask.remoteAddress),
							.accountId = responseTask.responsePacket.accountId,
							.persistentPlayerId = responseTask.persistentPlayerId,
							.nickname = responseTask.responsePacket.nickname,
							.currentTime = currentTime,
						};

						service::AccountLoginAdmissionService::Result admissionResult{};

						{
							std::scoped_lock lock(stateMutex_);

							admissionResult = accountLoginAdmissionService_.Apply(
								admissionRequest,
								authenticatedAccountRegistry_,
								peerRoomManager_
							);
						}

						protocol::ApplyAccountLoginAdmissionResult(admissionResult, responseTask.responsePacket);

						if (admissionResult.status == service::AccountLoginAdmissionService::Status::AlreadyLoggedIn)
						{
							LogWarning("Account login rejected because the account is already logged in.");
						}
						else if (admissionResult.status == service::AccountLoginAdmissionService::Status::TokenGenerationFailed)
						{
							LogError("Failed to generate account session token.");
						}
						else if (admissionResult.status == service::AccountLoginAdmissionService::Status::RegistrationFailed)
						{
							LogError("Failed to register authenticated account.");
						}
					}
				}
				else
				{
					LogDebug("Stale account login response did not modify authentication state.");
				}

				const bool finalized = accountLoginPacketHandler_->FinalizeResponse(responseTask.taskId, responseTask.responsePacket, currentTime);
				if (!finalized)
				{
					LogError("Failed to finalize account login response.");

					responseTask.responsePacket.status = common::packet::AccountLoginResponseStatus::ServerError;
					responseTask.responsePacket.accountId = 0;
					responseTask.responsePacket.sessionToken = common::net::invalidSessionToken;
					responseTask.responsePacket.nickname.clear();
					responseTask.persistentPlayerId = 0;
				}
			}

			if (packetSender_.SendAccountLoginResponse(responseTask.remoteAddress, responseTask.responsePacket))
			{
				continue;
			}

			LogWarning("Failed to send account login response.");
		}
	}

	void UdpServer::BroadcastPlayerSnapshots()
	{
		std::vector<protocol::PlayerSnapshotTask> playerSnapshotTaskList;

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
		std::vector<protocol::BulletSnapshotTask> bulletSnapshotTaskList;

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
		std::vector<protocol::ImpactEffectTask> impactEffectTaskList;

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
		std::size_t expiredAuthenticatedAccountCount = 0;
		std::size_t matchHistoryLeaveFailureCount = 0;

		{
			std::scoped_lock lock(stateMutex_);

			const common::time::TimePoint currentTime = common::time::Clock::now();
			const std::vector<service::PeerRoomManager::TimedOutPeer> timedOutPeerList = peerRoomManager_.RemoveTimedOutPeers(
				currentTime,
				config_.session.peerTimeout
			);

			expiredAuthenticatedAccountCount = authenticatedAccountRegistry_.RemoveExpired(currentTime, config_.session.peerTimeout);

			serverMetricsCollector_.AddTimedOutPeerCount(timedOutPeerList.size());

			timedOutBroadcastList.reserve(timedOutPeerList.size());

			const common::time::SystemTimePoint currentSystemTime = common::time::SystemClock::now();

			for (const service::PeerRoomManager::TimedOutPeer& timedOutPeer : timedOutPeerList)
			{
				const bool matchHistoryLeft = matchHistoryTracker_.LeavePlayer(
					timedOutPeer.roomId,
					timedOutPeer.persistentPlayerId,
					currentSystemTime
				);

				if (!matchHistoryLeft)
				{
					++matchHistoryLeaveFailureCount;
				}

				if (peerRoomManager_.GetRoomMemberCount(timedOutPeer.roomId) == 0)
				{
					gameWorld_.ClearRoomTransientState(timedOutPeer.roomId);
				}

				gameWorld_.RemovePlayer(timedOutPeer.playerId);

				timedOutBroadcastList.push_back(TimedOutBroadcast{
						.playerId = timedOutPeer.playerId,
						.roomId = timedOutPeer.roomId,
					});
			}
		}

		if (expiredAuthenticatedAccountCount > 0)
		{
			std::ostringstream stream;
			stream << "Expired authenticated accounts removed. Count="
				<< expiredAuthenticatedAccountCount;

			LogDebug(stream.str());
		}

		if (matchHistoryLeaveFailureCount > 0)
		{
			std::ostringstream stream;
			stream << "Failed to remove timed out players from match history."
				<< " Count=" << matchHistoryLeaveFailureCount;

			LogError(stream.str());
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

	void UdpServer::LogInvalidPacket(const sockaddr_in& remoteAddress, const protocol::UdpPacketDispatcher::DispatchResult& dispatchResult)
	{
		const diagnostics::InvalidPacketLogLimiter::LogDecision logDecision = invalidPacketLogLimiter_.Record(
			dispatchResult.status,
			common::time::Clock::now()
		);

		if (!logDecision.shouldLog)
		{
			return;
		}

		std::ostringstream stream;
		stream << "Invalid UDP packet dropped. "
			<< "Endpoint=" << FormatEndpoint(remoteAddress)
			<< ", Reason=" << protocol::UdpPacketDispatcher::ToString(dispatchResult.status)
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

		if (dispatchResult.status == protocol::UdpPacketDispatcher::DispatchStatus::InvalidPacketPayload
			&& dispatchResult.detailCode != 0)
		{
			const auto payloadValidationStatus = static_cast<protocol::PacketPayloadValidator::PayloadValidationStatus>(dispatchResult.detailCode);

			stream << ", PayloadReason=" << protocol::PacketPayloadValidator::ToString(payloadValidationStatus);
		}

		LogWarning(stream.str());
	}

	void UdpServer::LogServerStatusIfDue()
	{
		if (!serverStatusReporter_.ShouldReport(common::time::Clock::now()))
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
				[&reliablePendingPacketCount](const service::PeerState& peerState)
				{
					reliablePendingPacketCount += peerState.reliableSession.GetPendingPacketCount();
				}
			);

			snapshot.reliablePendingPacketCount = reliablePendingPacketCount;
		}

		const net::UdpIocpTransportMetricsSnapshot transportMetrics = udpTransport_.CaptureMetricsSnapshot();

		snapshot.pendingSendContextCount = udpTransport_.GetPendingSendContextCount();

		snapshot.faultSimulationPendingPacketCount = packetSender_.GetFaultSimulationPendingPacketCount();

		snapshot.udpSendCompletionCount = transportMetrics.sendCompletionCount;
		snapshot.udpSendCompletionFailureCount = transportMetrics.sendCompletionFailureCount;
		snapshot.udpSendCompletedByteCount = transportMetrics.sendCompletedByteCount;

		snapshot.metrics = serverMetricsCollector_.CaptureSnapshot();

		return snapshot;
	}
}