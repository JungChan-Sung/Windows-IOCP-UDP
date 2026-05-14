#pragma once

#include <WinSock2.h>

#include <chrono>
#include <cstdint>

#include <Common/Game/GameTypes.h>
#include <Common/Net/Endpoint.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameWorld.h>
#include <Server/Net/PeerRoomManager.h>

namespace server::net
{
	class PeerSessionService
	{
	public:
		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;
		using EndpointKey = common::net::EndpointKey;
		using TimePoint = std::chrono::steady_clock::time_point;

	public:
		struct JoinResult
		{
		public:
			bool shouldSendResponse = false;
			bool shouldBroadcastPlayerJoined = false;

			sockaddr_in remoteAddress{};
			PlayerId playerId = 0;
			RoomId roomId = 0;
			game::GameSimulation::SpawnPosition spawnPosition{};
		};

		struct LeaveResult
		{
		public:
			bool shouldBroadcastPlayerLeft = false;

			PlayerId playerId = 0;
			RoomId roomId = 0;
		};

		struct RoomChangeResult
		{
		public:
			bool changed = false;

			PlayerId playerId = 0;
			RoomId previousRoomId = 0;
			RoomId nextRoomId = 0;
			sockaddr_in remoteAddress{};
			game::GameSimulation::SpawnPosition spawnPosition{};
		};

	public:
		PeerSessionService() = default;
		~PeerSessionService() noexcept = default;

		PeerSessionService(const PeerSessionService&) = delete;
		PeerSessionService& operator=(const PeerSessionService&) = delete;

		PeerSessionService(PeerSessionService&&) = delete;
		PeerSessionService& operator=(PeerSessionService&&) = delete;

	public:
		[[nodiscard]] JoinResult JoinPeer(const sockaddr_in& remoteAddress,
			const EndpointKey& endpointKey,
			RoomId initialRoomId,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			const game::GameSimulation& gameSimulation,
			const config::GameRuleConfig& gameRuleConfig, 
			TimePoint currentTime
		) const;
		[[nodiscard]] LeaveResult LeavePeer(
			const EndpointKey& endpointKey,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld
		) const;
		[[nodiscard]] RoomChangeResult ChangePeerRoom(
			const EndpointKey& endpointKey,
			RoomId nextRoomId,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			const game::GameSimulation& gameSimulation,
			TimePoint currentTime
		) const;

	private:
		[[nodiscard]] game::PlayerState CreateInitialPlayerState(
			PlayerId playerId,
			const game::GameSimulation::SpawnPosition& spawnPosition,
			const config::GameRuleConfig& gameRuleConfig
		) const noexcept;
	};
}
