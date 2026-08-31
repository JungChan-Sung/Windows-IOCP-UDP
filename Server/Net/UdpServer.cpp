#include "UdpServer.h"

#include <WS2tcpip.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <variant>

#include <Common/Log/ILogger.h>
#include <Common/Net/Reliable/ReliableUdpPacketBuilder.h>
#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Packet/Control/ControlPacket.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketReliability.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/String/StringFormat.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Account/AccountService.h>
#include <Server/Config/ServerConfigValidator.h>
#include <Server/Protocol/AccountLoginPacketHandler.h>
#include <Server/Protocol/AccountPacketMapper.h>
#include <Server/Protocol/GamePacketMapper.h>
#include <Server/Protocol/PacketPayloadValidator.h>

namespace
{
	template <typename TObject>
	using EndpointOnlyPacketHandler = void (TObject::*)(const common::net::EndpointKey&);

	template <typename TObject, typename TPacket>
	using EndpointTypedPacketHandler = void (TObject::*)(const common::net::EndpointKey&, const TPacket&);

	template <typename TObject>
	void RegisterEndpointOnlyPacketHandler(
		server::protocol::UdpPacketDispatcher& packetDispatcher,
		common::packet::PacketType packetType,
		TObject& object,
		int expectedPacketSize,
		EndpointOnlyPacketHandler<TObject> handler
	)
	{
		using DispatchStatus = server::protocol::UdpPacketDispatcher::DispatchStatus;
		using PacketProcessResult = server::protocol::UdpPacketDispatcher::PacketProcessResult;

		packetDispatcher.RegisterHandler(
			packetType,
			expectedPacketSize,
			[&object, handler](const common::net::EndpointKey& endpointKey, const char*, int)
			{
				std::invoke(handler, object, endpointKey);
				return PacketProcessResult{ DispatchStatus::Succeeded, 0 };
			}
		);
	}

	template <typename TObject, typename TPacket>
	void RegisterTypedPacketHandler(
		server::protocol::UdpPacketDispatcher& packetDispatcher,
		common::packet::PacketType packetType,
		TObject& object,
		EndpointTypedPacketHandler<TObject, TPacket> handler
	)
	{
		using Packet = std::remove_cvref_t<TPacket>;
		using DispatchStatus = server::protocol::UdpPacketDispatcher::DispatchStatus;
		using PacketProcessResult = server::protocol::UdpPacketDispatcher::PacketProcessResult;

		packetDispatcher.RegisterHandler(
			packetType,
			common::packet::packetExpectedSize<Packet>,
			[&object, handler](const common::net::EndpointKey& endpointKey, const char* packetData, int packetSize)
			{
				std::optional<Packet> packet = common::packet::DeserializePacket<Packet>(packetData, packetSize);
				if (!packet.has_value())
				{
					return PacketProcessResult{ DispatchStatus::InvalidPacketPayload, 0 };
				}

				std::invoke(handler, object, endpointKey, *packet);
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
		EndpointTypedPacketHandler<TObject, TPacket> handler
	)
	{
		using Packet = std::remove_cvref_t<TPacket>;
		using DispatchStatus = server::protocol::UdpPacketDispatcher::DispatchStatus;
		using PacketProcessResult = server::protocol::UdpPacketDispatcher::PacketProcessResult;
		using PayloadValidationStatus = server::protocol::PacketPayloadValidator::PayloadValidationStatus;

		packetDispatcher.RegisterHandler(
			packetType,
			common::packet::packetExpectedSize<Packet>,
			[&object, validator, handler](const common::net::EndpointKey& endpointKey, const char* packetData, int packetSize)
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

				std::invoke(handler, object, endpointKey, *packet);
				return PacketProcessResult{ DispatchStatus::Succeeded, 0 };
			}
		);
	}

	template <typename TPacket>
	[[nodiscard]] bool SendSerializedPacket(
		server::net::UdpPacketSender& packetSender,
		const common::net::EndpointKey& endpointKey,
		const TPacket& packet
	)
	{
		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return false;
		}

		return packetSender.SendPacket(endpointKey, packetBuffer->data(), static_cast<int>(packetBuffer->size()));
	}

	template <typename TPacket>
	[[nodiscard]] std::size_t BroadcastSerializedPacket(
		server::net::UdpPacketSender& packetSender,
		std::span<const common::net::EndpointKey> endpointKeyList,
		const TPacket& packet
	)
	{
		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return 0;
		}

		return packetSender.BroadcastPacket(endpointKeyList, packetBuffer->data(), static_cast<int>(packetBuffer->size()));
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

	[[nodiscard]] std::string FormatEndpoint(const common::net::EndpointKey& endpointKey)
	{
		return FormatEndpoint(common::net::MakeSocketAddress(endpointKey));
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

				const EndpointKey endpointKey = common::net::MakeEndpointKey(remoteAddress);
				const protocol::UdpPacketDispatcher::DispatchResult dispatchResult = DispatchPacket(
					endpointKey,
					packetData,
					packetSize
				);
				if (dispatchResult.status != protocol::UdpPacketDispatcher::DispatchStatus::Succeeded)
				{
					serverMetricsCollector_.IncrementInvalidPacketDropCount();
					LogInvalidPacket(endpointKey, dispatchResult);
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
			reliableUdpSessionRegistry_.Clear();
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

	diagnostics::ServerStatusSnapshot UdpServer::CaptureStatusSnapshot() const
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

			snapshot.reliablePendingPacketCount = reliableUdpSessionRegistry_.GetPendingPacketCount();
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

	diagnostics::ServerDetailSnapshot UdpServer::CaptureDetailSnapshot() const
	{
		diagnostics::ServerDetailSnapshot snapshot{};

		std::scoped_lock lock(stateMutex_);

		snapshot.playerList.reserve(peerRoomManager_.GetJoinedPeerCount());
		snapshot.roomList.reserve(peerRoomManager_.GetRoomCount());

		std::unordered_map<RoomId, std::size_t> roomMemberCountTable;
		roomMemberCountTable.reserve(peerRoomManager_.GetRoomCount());
		peerRoomManager_.ForEachJoinedPeer(
			[&snapshot, &roomMemberCountTable](const service::PeerState& peerState)
			{
				snapshot.playerList.push_back(diagnostics::ServerPlayerDetailSnapshot{
					.playerId = peerState.playerId,
					.accountId = peerState.accountId,
					.persistentPlayerId = peerState.persistentPlayerId,
					.nickname = peerState.nickname,
					.roomId = peerState.roomId,
					.lastAcceptedInputSequence = peerState.lastAcceptedInputSequence,
					.lastProcessedInputSequence = peerState.lastProcessedInputSequence,
					});

				++roomMemberCountTable[peerState.roomId];
			}
		);

		for (const auto& [roomId, memberCount] : roomMemberCountTable)
		{
			snapshot.roomList.push_back(diagnostics::ServerRoomDetailSnapshot{
				.roomId = roomId,
				.memberCount = memberCount,
				});
		}

		std::ranges::sort(snapshot.playerList, {}, &diagnostics::ServerPlayerDetailSnapshot::playerId);
		std::ranges::sort(snapshot.roomList, {}, &diagnostics::ServerRoomDetailSnapshot::roomId);

		return snapshot;
	}

	UdpServer::KickPlayerResult UdpServer::KickPlayer(PlayerId playerId)
	{
		KickPlayerResult result{};

		if (playerId == 0)
		{
			return result;
		}

		EndpointKey endpointKey{};
		service::PeerSessionService::LeaveResult leaveResult{};

		std::optional<common::packet::PacketBuffer> disconnectPacketBuffer;

		bool matchHistoryLeft = true;
		bool authenticatedAccountRemoved = true;

		{
			std::scoped_lock lock(stateMutex_);

			const service::PeerState* peerState = peerRoomManager_.FindJoinedPeerByPlayerId(playerId);
			if (peerState == nullptr)
			{
				return result;
			}

			endpointKey = peerState->endpointKey;
			leaveResult = peerSessionService_.LeavePeer(
				endpointKey,
				peerRoomManager_,
				gameWorld_
			);

			if (!leaveResult.shouldBroadcastPlayerLeft)
			{
				return result;
			}

			result.kicked = true;
			result.playerId = leaveResult.playerId;
			result.roomId = leaveResult.roomId;

			matchHistoryLeft = matchHistoryTracker_.LeavePlayer(
				leaveResult.roomId,
				leaveResult.persistentPlayerId,
				common::time::SystemClock::now()
			);

			authenticatedAccountRemoved = authenticatedAccountRegistry_.Remove(endpointKey);

			disconnectPacketBuffer = BuildReliableServerDisconnect(
				endpointKey,
				common::packet::ServerDisconnectReason::Kicked
			);
			if (disconnectPacketBuffer.has_value())
			{
				result.disconnectNotificationQueued = true;
				static_cast<void>(reliableUdpSessionRegistry_.BeginClose(endpointKey));
			}
			else
			{
				static_cast<void>(reliableUdpSessionRegistry_.Remove(endpointKey));
			}
		}

		if (!matchHistoryLeft)
		{
			LogError("Failed to remove kicked player from match history.");
		}

		if (!authenticatedAccountRemoved)
		{
			LogWarning("Kicked player's authenticated account state was not found.");
		}

		if (disconnectPacketBuffer.has_value())
		{
			if (!packetSender_.SendPacket(endpointKey, disconnectPacketBuffer->data(), static_cast<int>(disconnectPacketBuffer->size())))
			{
				LogWarning("Initial server disconnect notification send failed. Packet remains queued for retry.");
			}
		}
		else
		{
			LogWarning("Failed to queue server disconnect notification for kicked player.");
		}

		{
			std::ostringstream stream;
			stream << "Player kicked. PlayerId=" << result.playerId << ", RoomId=" << result.roomId;
			LogInfo(stream.str());
		}

		BroadcastPlayerLeft(result.roomId, result.playerId);

		return result;
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

			CommitProcessedInputSequences();

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

		BroadcastSnapshots();

		LogServerStatusIfDue();
	}

	void UdpServer::CommitProcessedInputSequences() noexcept
	{
		peerRoomManager_.ForEachJoinedPeer(
			[](service::PeerState& peerState)
			{
				peerState.lastProcessedInputSequence = peerState.lastAcceptedInputSequence;
			}
		);
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

	protocol::SnapshotBroadcastContext UdpServer::BuildSnapshotBroadcastContext() const
	{
		protocol::SnapshotBroadcastContext context{};
		context.serverTick = gameWorld_.GetServerTick();

		const common::time::Milliseconds serverTickInterval = std::chrono::duration_cast<common::time::Milliseconds>(config_.tick.tickInterval);
		context.serverTickIntervalMilliseconds = static_cast<std::uint32_t>(serverTickInterval.count());

		context.roomContextList.reserve(peerRoomManager_.GetRoomCount());

		std::unordered_map<RoomId, std::size_t> roomIndexTable;
		roomIndexTable.reserve(peerRoomManager_.GetRoomCount());
		peerRoomManager_.ForEachJoinedPeer(
			[this, &context, &roomIndexTable](const service::PeerState& peerState)
			{
				const auto [roomIterator, inserted] = roomIndexTable.try_emplace(peerState.roomId, context.roomContextList.size());
				if (inserted)
				{
					context.roomContextList.push_back(protocol::SnapshotRoomContext{
						.roomId = peerState.roomId,
						});
				}

				protocol::SnapshotRoomContext& roomContext = context.roomContextList[roomIterator->second];
				roomContext.peerContextList.push_back(protocol::SnapshotPeerContext{
					.endpointKey = peerState.endpointKey,
					.playerId = peerState.playerId,
					.lastProcessedInputSequence = peerState.lastProcessedInputSequence,
					});

				const game::PlayerState* playerState = gameWorld_.FindPlayer(peerState.playerId);
				if (playerState == nullptr)
				{
					return;
				}

				roomContext.playerStateContextList.push_back(protocol::SnapshotPlayerStateContext{
					.playerId = playerState->playerId,
					.x = playerState->x,
					.y = playerState->y,
					.hp = playerState->hp,
					.isDead = playerState->isDead,
					.killCount = playerState->killCount,
					.deathCount = playerState->deathCount,
					.respawnRemainingSeconds = playerState->respawnRemainingSeconds,
					.invincibilityRemainingSeconds = playerState->invincibilityRemainingSeconds,
					.hitFlashRemainingSeconds = playerState->hitFlashRemainingSeconds,
					});
			}
		);

		for (const game::BulletState& bulletState : gameWorld_.GetBulletStateList())
		{
			const auto roomIterator = roomIndexTable.find(bulletState.roomId);
			if (roomIterator == roomIndexTable.end())
			{
				continue;
			}

			context.roomContextList[roomIterator->second].bulletStateContextList.push_back(protocol::SnapshotBulletStateContext{
				.bulletId = bulletState.bulletId,
				.x = bulletState.x,
				.y = bulletState.y,
				});
		}

		for (const game::ImpactEffectState& effectState : gameWorld_.GetPendingImpactEffectStateList())
		{
			const auto roomIterator = roomIndexTable.find(effectState.roomId);
			if (roomIterator == roomIndexTable.end())
			{
				continue;
			}

			context.roomContextList[roomIterator->second].impactEffectContextList.push_back(protocol::SnapshotImpactEffectContext{
				.effectType = effectState.effectType,
				.x = effectState.x,
				.y = effectState.y,
				});
		}

		return context;
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

		RegisterEndpointOnlyPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::FireRequest,
			*this,
			common::packet::packetExpectedSize<common::packet::FireRequestPacket>,
			&UdpServer::HandleFireRequest
		);

		RegisterEndpointOnlyPacketHandler(
			packetDispatcher_,
			common::packet::PacketType::KeepAlive,
			*this,
			common::packet::packetExpectedSize<common::packet::KeepAlivePacket>,
			&UdpServer::HandleKeepAlive
		);

		RegisterEndpointOnlyPacketHandler(
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

	protocol::UdpPacketDispatcher::DispatchResult UdpServer::DispatchPacket(const EndpointKey& endpointKey, const char* packetData, int packetSize)
	{
		using DispatchResult = protocol::UdpPacketDispatcher::DispatchResult;
		using DispatchStatus = protocol::UdpPacketDispatcher::DispatchStatus;

		const std::optional<common::packet::PacketHeader> packetHeader = common::packet::DeserializePacketHeader(packetData, packetSize);
		if (!packetHeader.has_value())
		{
			return packetDispatcher_.Dispatch(endpointKey, packetData, packetSize);
		}

		const bool isReliable = common::packet::IsReliablePacketHeader(*packetHeader);
		if (!common::packet::IsPacketTransportReliabilityValid(packetHeader->type, isReliable))
		{
			return DispatchResult{ DispatchStatus::InvalidPacketHeader, packetHeader->type, packetSize };
		}

		if (isReliable)
		{
			return DispatchReliablePacket(endpointKey, packetData, packetSize);
		}

		return packetDispatcher_.Dispatch(endpointKey, packetData, packetSize);
	}

	protocol::UdpPacketDispatcher::DispatchResult UdpServer::DispatchReliablePacket(const EndpointKey& endpointKey, const char* packetData, int packetSize)
	{
		using DispatchResult = protocol::UdpPacketDispatcher::DispatchResult;
		using DispatchStatus = protocol::UdpPacketDispatcher::DispatchStatus;
		using ProcessStatus = ReliableUdpSessionRegistry::ProcessReceivedPacketStatus;

		const std::optional<common::net::ReliableUdpPacketView> packetView = common::net::ParseReliableUdpPacket(packetData, packetSize);
		if (!packetView.has_value())
		{
			serverMetricsCollector_.IncrementInvalidReliablePacketCount();
			return DispatchResult{ DispatchStatus::InvalidPacketHeader, std::nullopt, packetSize };
		}

		const bool isAckOnlyPacket = packetView->packetHeader.type == common::packet::PacketType::None;

		ReliableUdpSessionRegistry::ProcessReceivedPacketResult processResult{};

		{
			std::scoped_lock lock(stateMutex_);
			processResult = reliableUdpSessionRegistry_.ProcessReceivedPacket(endpointKey, *packetView);
		}

		if (processResult.status == ProcessStatus::SessionNotFound)
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

		switch (processResult.status)
		{
		case ProcessStatus::AckOnlyProcessed:
			serverMetricsCollector_.IncrementReliableAckOnlyReceivePacketCount();
			return DispatchResult{ DispatchStatus::Succeeded, packetView->packetHeader.type, packetSize };

		case ProcessStatus::InvalidAck:
			serverMetricsCollector_.IncrementReliableInvalidAckPacketCount();
			serverMetricsCollector_.IncrementReliableAckOnlyReceivePacketCount();
			return DispatchResult{ DispatchStatus::Succeeded, packetView->packetHeader.type, packetSize };

		case ProcessStatus::DataReceived:
		case ProcessStatus::DuplicateData:
			serverMetricsCollector_.IncrementReliableDataReceivePacketCount();
			break;

		case ProcessStatus::SessionNotFound:
			break;
		}

		if (processResult.ackPacketBuffer.has_value())
		{
			static_cast<void>(packetSender_.SendPacket(
				endpointKey,
				processResult.ackPacketBuffer->data(),
				static_cast<int>(processResult.ackPacketBuffer->size())
			));

			serverMetricsCollector_.IncrementReliableAckOnlySendPacketCount();
		}

		if (processResult.status == ProcessStatus::DuplicateData)
		{
			serverMetricsCollector_.IncrementReliableDuplicateDropPacketCount();
			return DispatchResult{ DispatchStatus::Succeeded, packetView->packetHeader.type, packetSize };
		}

		const std::optional<common::packet::PacketBuffer> gamePacketBuffer = common::net::BuildGamePacketFromReliableUdpPacketView(*packetView);
		if (!gamePacketBuffer.has_value())
		{
			return DispatchResult{ DispatchStatus::InvalidPacketPayload, packetView->packetHeader.type, packetSize };
		}

		return packetDispatcher_.Dispatch(endpointKey, gamePacketBuffer->data(), static_cast<int>(gamePacketBuffer->size()));
	}

	std::optional<common::packet::PacketBuffer> UdpServer::BuildReliableJoinRoomResponse(const EndpointKey& endpointKey, const service::PeerSessionService::RoomChangeResult& roomChangeResult)
	{
		const common::packet::JoinRoomResponsePacket responsePacket = protocol::BuildJoinRoomResponse(roomChangeResult);
		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(responsePacket);
		if (!packetBuffer.has_value())
		{
			return std::nullopt;
		}

		ReliableUdpSessionRegistry::BuildOutgoingPacketResult buildResult = reliableUdpSessionRegistry_.BuildOutgoingPacket(
			endpointKey,
			common::packet::ConstPacketSpan(packetBuffer->data(), packetBuffer->size()),
			common::time::Clock::now()
		);
		if (!buildResult.has_value())
		{
			if (buildResult.error() == ReliableUdpSessionRegistry::BuildOutgoingPacketFailure::SendWindowFull)
			{
				serverMetricsCollector_.IncrementReliableSendWindowFullCount();
			}

			return std::nullopt;
		}

		serverMetricsCollector_.IncrementReliableDataSendPacketCount();

		return std::move(*buildResult);
	}

	std::optional<common::packet::PacketBuffer> UdpServer::BuildReliableLeaveResponse(const EndpointKey& endpointKey)
	{
		const common::packet::LeaveResponsePacket responsePacket{};
		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(responsePacket);
		if (!packetBuffer.has_value())
		{
			return std::nullopt;
		}

		ReliableUdpSessionRegistry::BuildOutgoingPacketResult buildResult = reliableUdpSessionRegistry_.BuildOutgoingPacket(
			endpointKey,
			common::packet::ConstPacketSpan(packetBuffer->data(), packetBuffer->size()),
			common::time::Clock::now()
		);
		if (!buildResult.has_value())
		{
			if (buildResult.error() == ReliableUdpSessionRegistry::BuildOutgoingPacketFailure::SendWindowFull)
			{
				serverMetricsCollector_.IncrementReliableSendWindowFullCount();
			}

			return std::nullopt;
		}

		serverMetricsCollector_.IncrementReliableDataSendPacketCount();

		return std::move(*buildResult);
	}

	std::optional<common::packet::PacketBuffer> UdpServer::BuildReliableServerDisconnect(const EndpointKey& endpointKey, common::packet::ServerDisconnectReason reason)
	{
		common::packet::ServerDisconnectPacket packet{};
		packet.reason = reason;

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		if (!packetBuffer.has_value())
		{
			return std::nullopt;
		}

		ReliableUdpSessionRegistry::BuildOutgoingPacketResult buildResult = reliableUdpSessionRegistry_.BuildOutgoingPacket(
			endpointKey,
			common::packet::ConstPacketSpan(packetBuffer->data(), packetBuffer->size()),
			common::time::Clock::now()
		);
		if (!buildResult.has_value())
		{
			if (buildResult.error() == ReliableUdpSessionRegistry::BuildOutgoingPacketFailure::SendWindowFull)
			{
				serverMetricsCollector_.IncrementReliableSendWindowFullCount();
			}

			return std::nullopt;
		}

		serverMetricsCollector_.IncrementReliableDataSendPacketCount();

		return std::move(*buildResult);
	}

	void UdpServer::HandleJoinRequest(const EndpointKey& endpointKey, const common::packet::JoinRequestPacket& packet)
	{
		serverMetricsCollector_.IncrementJoinRequestCount();
		ProcessJoinRequest(endpointKey, packet);
	}

	void UdpServer::HandleInputCommand(const EndpointKey& endpointKey, const common::packet::InputCommandPacket& packet)
	{
		serverMetricsCollector_.IncrementInputCommandCount();
		ProcessInputCommand(endpointKey, packet);
	}

	void UdpServer::HandleFireRequest(const EndpointKey& endpointKey)
	{
		serverMetricsCollector_.IncrementFireRequestCount();
		ProcessFireRequest(endpointKey);
	}

	void UdpServer::HandleKeepAlive(const EndpointKey& endpointKey)
	{
		std::scoped_lock lock(stateMutex_);

		static_cast<void>(peerRoomManager_.RefreshRecvTime(endpointKey, common::time::Clock::now()));
	}

	void UdpServer::HandleLeaveRequest(const EndpointKey& endpointKey)
	{
		serverMetricsCollector_.IncrementLeaveRequestCount();
		ProcessLeaveRequest(endpointKey);
	}

	void UdpServer::HandleJoinRoomRequest(const EndpointKey& endpointKey, const common::packet::JoinRoomRequestPacket& packet)
	{
		serverMetricsCollector_.IncrementJoinRoomRequestCount();
		ProcessJoinRoomRequest(endpointKey, packet);
	}

	void UdpServer::HandleAccountLoginRequest(const EndpointKey& endpointKey, const common::packet::AccountLoginRequestPacket& packet)
	{
		if (accountLoginPacketHandler_ == nullptr)
		{
			return;
		}

		const protocol::AccountLoginPacketHandler::EnqueueStatus enqueueStatus = accountLoginPacketHandler_->Enqueue(
			endpointKey,
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

		const common::packet::AccountLoginResponsePacket responsePacket = protocol::BuildAccountLoginServerErrorResponse(packet.requestId);
		if (!SendSerializedPacket(packetSender_, endpointKey, responsePacket))
		{
			LogWarning("Failed to send account login server error response.");
		}
	}

	void UdpServer::ProcessReliableResends()
	{
		ReliableUdpSessionRegistry::ResendBatch resendBatch{};

		{
			std::scoped_lock lock(stateMutex_);
			resendBatch = reliableUdpSessionRegistry_.ExtractResendBatch(common::time::Clock::now());
		}

		for (const ReliableUdpSessionRegistry::ResendTask& resendTask : resendBatch.taskList)
		{
			static_cast<void>(packetSender_.SendPacket(
				resendTask.endpointKey,
				resendTask.packetBuffer.data(),
				static_cast<int>(resendTask.packetBuffer.size())
			));
		}

		serverMetricsCollector_.AddReliableResendPacketCount(static_cast<std::uint64_t>(resendBatch.taskList.size()));
		serverMetricsCollector_.AddReliableResendGiveUpPacketCount(static_cast<std::uint64_t>(resendBatch.giveUpPacketCount));

		if (resendBatch.giveUpPacketCount > 0)
		{
			std::ostringstream stream;
			stream << "Reliable resend give-up packets detected. Count=" << resendBatch.giveUpPacketCount;
			LogWarning(stream.str());
		}
	}

	void UdpServer::ProcessJoinRequest(const EndpointKey& endpointKey, const common::packet::JoinRequestPacket& packet)
	{
		using JoinStatus = service::PeerSessionService::JoinAuthenticatedPeerStatus;

		service::PeerSessionService::JoinAuthenticatedPeerResult authenticatedJoinResult{};
		bool matchHistoryEntered = true;

		{
			std::scoped_lock lock(stateMutex_);

			authenticatedJoinResult = peerSessionService_.JoinAuthenticatedPeer(
				endpointKey,
				packet.sessionToken,
				config_.session.initialRoomId,
				authenticatedAccountRegistry_,
				peerRoomManager_,
				gameWorld_,
				config_.gameRule,
				common::time::Clock::now()
			);
			if (authenticatedJoinResult.status == JoinStatus::Joined)
			{
				const service::PeerSessionService::JoinResult& joinResult = authenticatedJoinResult.joinResult;

				static_cast<void>(reliableUdpSessionRegistry_.Upsert(endpointKey, config_.reliableUdp));

				matchHistoryEntered = matchHistoryTracker_.EnterPlayer(
					joinResult.roomId,
					joinResult.persistentPlayerId,
					common::time::SystemClock::now()
				);
			}
		}

		if (!matchHistoryEntered)
		{
			LogError("Failed to enter player into match history.");
		}

		if (authenticatedJoinResult.status == JoinStatus::Unauthenticated)
		{
			std::ostringstream stream;
			stream << "Unauthenticated join request ignored. Endpoint=" << FormatEndpoint(endpointKey);
			LogWarning(stream.str());
			return;
		}

		if (authenticatedJoinResult.status == JoinStatus::Rejected)
		{
			std::ostringstream stream;
			stream << "Join request rejected because the session identity did not match. Endpoint=" << FormatEndpoint(endpointKey);
			LogWarning(stream.str());
			return;
		}

		const service::PeerSessionService::JoinResult& joinResult = authenticatedJoinResult.joinResult;
		const common::packet::JoinResponsePacket responsePacket = protocol::BuildJoinResponse(joinResult);
		const bool responseSent = SendSerializedPacket(packetSender_, endpointKey, responsePacket);
		if (!responseSent)
		{
			std::ostringstream stream;
			stream << "Join response send failed. Endpoint=" << FormatEndpoint(endpointKey)
				<< ", PlayerId=" << joinResult.playerId
				<< ", RoomId=" << joinResult.roomId;

			LogWarning(stream.str());
		}

		if (authenticatedJoinResult.status == JoinStatus::Joined)
		{
			{
				std::ostringstream stream;
				stream << "Peer joined. Endpoint=" << FormatEndpoint(endpointKey)
					<< ", PlayerId=" << joinResult.playerId
					<< ", RoomId=" << joinResult.roomId;

				LogInfo(stream.str());
			}

			BroadcastPlayerJoined(joinResult.roomId, joinResult.playerId, joinResult.spawnPosition.x, joinResult.spawnPosition.y);
			return;
		}

		if (responseSent)
		{
			std::ostringstream stream;
			stream << "Join response sent to existing peer. Endpoint=" << FormatEndpoint(endpointKey)
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
		std::optional<common::packet::PacketBuffer> leaveResponsePacketBuffer;
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

				leaveResponsePacketBuffer = BuildReliableLeaveResponse(endpointKey);
				if (leaveResponsePacketBuffer.has_value())
				{
					static_cast<void>(reliableUdpSessionRegistry_.BeginClose(endpointKey));
				}
				else
				{
					static_cast<void>(reliableUdpSessionRegistry_.Remove(endpointKey));
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

		if (leaveResponsePacketBuffer.has_value())
		{
			static_cast<void>(packetSender_.SendPacket(
				endpointKey,
				leaveResponsePacketBuffer->data(),
				static_cast<int>(leaveResponsePacketBuffer->size())
			));
		}
		else
		{
			LogWarning("Failed to build reliable leave response.");
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

				nextMatchEntered = matchHistoryTracker_.EnterPlayer(
					roomChangeResult.nextRoomId,
					roomChangeResult.persistentPlayerId,
					currentSystemTime
				);

				reliableResponsePacketBuffer = BuildReliableJoinRoomResponse(endpointKey, roomChangeResult);
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
			static_cast<void>(packetSender_.SendPacket(
				endpointKey,
				reliableResponsePacketBuffer->data(),
				static_cast<int>(reliableResponsePacketBuffer->size())
			));
		}
		else
		{
			std::ostringstream stream;
			stream << "Failed to build reliable join room response. Endpoint=" << FormatEndpoint(endpointKey)
				<< ", PlayerId=" << roomChangeResult.playerId
				<< ", RoomId=" << roomChangeResult.nextRoomId;

			LogWarning(stream.str());
		}

		{
			std::ostringstream stream;
			stream << "Peer changed room. Endpoint=" << FormatEndpoint(endpointKey)
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
					if (responseTask.accountLoginRecord.has_value())
					{
						const account::AccountLoginRecord& accountLoginRecord = *responseTask.accountLoginRecord;
						const service::AccountLoginAdmissionService::Request admissionRequest{
							.endpointKey = responseTask.endpointKey,
							.accountId = accountLoginRecord.accountId,
							.persistentPlayerId = accountLoginRecord.persistentPlayerId,
							.nickname = accountLoginRecord.nickname,
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

					responseTask.responsePacket = protocol::BuildAccountLoginServerErrorResponse(responseTask.responsePacket.requestId);
					responseTask.accountLoginRecord.reset();
				}
			}

			if (SendSerializedPacket(packetSender_, responseTask.endpointKey, responseTask.responsePacket))
			{
				continue;
			}

			LogWarning("Failed to send account login response.");
		}
	}

	void UdpServer::BroadcastSnapshots()
	{
		protocol::SnapshotBroadcastContext context;

		{
			std::scoped_lock lock(stateMutex_);

			context = BuildSnapshotBroadcastContext();
			gameWorld_.ClearPendingImpactEffects();
		}

		const std::vector<protocol::PlayerSnapshotTask> playerTaskList = snapshotBroadcastBuilder_.BuildPlayerSnapshotTasks(context);
		const std::vector<protocol::BulletSnapshotTask> bulletTaskList = snapshotBroadcastBuilder_.BuildBulletSnapshotTasks(context);
		const std::vector<protocol::ImpactEffectTask> impactEffectTaskList = snapshotBroadcastBuilder_.BuildImpactEffectTasks(context);

		std::size_t playerSentCount = 0;

		for (const protocol::PlayerSnapshotTask& task : playerTaskList)
		{
			if (SendSerializedPacket(packetSender_, task.endpointKey, task.snapshotPacket))
			{
				++playerSentCount;
			}
		}

		std::size_t bulletSentCount = 0;

		for (const protocol::BulletSnapshotTask& task : bulletTaskList)
		{
			bulletSentCount += BroadcastSerializedPacket(
				packetSender_,
				task.endpointKeyList,
				task.snapshotPacket
			);
		}

		std::size_t impactEffectSentCount = 0;

		for (const protocol::ImpactEffectTask& task : impactEffectTaskList)
		{
			impactEffectSentCount += BroadcastSerializedPacket(
				packetSender_,
				task.endpointKeyList,
				task.effectPacket
			);
		}

		serverMetricsCollector_.AddPlayerSnapshotSendRequestCount(static_cast<std::uint64_t>(playerSentCount));
		serverMetricsCollector_.AddBulletSnapshotSendRequestCount(static_cast<std::uint64_t>(bulletSentCount));
		serverMetricsCollector_.AddImpactEffectSendRequestCount(static_cast<std::uint64_t>(impactEffectSentCount));
	}

	void UdpServer::BroadcastPlayerJoined(RoomId roomId, PlayerId playerId, float x, float y)
	{
		service::PeerRoomManager::EndpointKeyList endpointKeyList = [&]()
			{
				std::scoped_lock lock(stateMutex_);
				return peerRoomManager_.BuildRoomEndpointKeyList(roomId);
			}();

		const common::packet::PlayerJoinedPacket packet = protocol::BuildPlayerJoined(playerId, roomId, x, y);
		static_cast<void>(BroadcastSerializedPacket(packetSender_, endpointKeyList, packet));
	}

	void UdpServer::BroadcastPlayerLeft(RoomId roomId, PlayerId playerId)
	{
		service::PeerRoomManager::EndpointKeyList endpointKeyList = [&]()
			{
				std::scoped_lock lock(stateMutex_);
				return peerRoomManager_.BuildRoomEndpointKeyList(roomId);
			}();

		const common::packet::PlayerLeftPacket packet = protocol::BuildPlayerLeft(playerId, roomId);
		static_cast<void>(BroadcastSerializedPacket(packetSender_, endpointKeyList, packet));
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
			const service::PeerSessionService::TimedOutPeerList timedOutPeerList = peerSessionService_.RemoveTimedOutPeers(
				currentTime,
				config_.session.peerTimeout,
				peerRoomManager_,
				gameWorld_
			);

			expiredAuthenticatedAccountCount = authenticatedAccountRegistry_.RemoveExpired(currentTime, config_.session.peerTimeout);

			serverMetricsCollector_.AddTimedOutPeerCount(timedOutPeerList.size());

			timedOutBroadcastList.reserve(timedOutPeerList.size());

			const common::time::SystemTimePoint currentSystemTime = common::time::SystemClock::now();

			for (const service::PeerRoomManager::TimedOutPeer& timedOutPeer : timedOutPeerList)
			{
				static_cast<void>(reliableUdpSessionRegistry_.Remove(timedOutPeer.endpointKey));

				const bool matchHistoryLeft = matchHistoryTracker_.LeavePlayer(
					timedOutPeer.roomId,
					timedOutPeer.persistentPlayerId,
					currentSystemTime
				);

				if (!matchHistoryLeft)
				{
					++matchHistoryLeaveFailureCount;
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

	void UdpServer::LogInvalidPacket(const EndpointKey& endpointKey, const protocol::UdpPacketDispatcher::DispatchResult& dispatchResult)
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
			<< "Endpoint=" << FormatEndpoint(endpointKey)
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

		const diagnostics::ServerStatusSnapshot snapshot = CaptureStatusSnapshot();
		LogInfo(serverStatusReporter_.BuildMessage(snapshot));
	}
}