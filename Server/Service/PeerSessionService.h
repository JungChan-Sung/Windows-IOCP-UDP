#pragma once


#include <cstdint>
#include <string_view>

#include <Common/Game/GameTypes.h>
#include <Common/Game/GameRules.h>
#include <Common/Game/RoomLayout.h>
#include <Common/Identity/IdentityTypes.h>
#include <Common/Net/EndpointKey.h>
#include <Common/Net/SessionToken.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Game/GameWorld.h>
#include <Server/Service/PeerRoomManager.h>

namespace server::service
{
	class AuthenticatedAccountRegistry;

	class PeerSessionService
	{
	public:
		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;
		using EndpointKey = common::net::EndpointKey;
		using TimePoint = common::time::TimePoint;
		using Duration = common::time::Duration;

		using RecoverablePeer = PeerRoomManager::RecoverablePeer;
		using RecoverablePeerList = PeerRoomManager::RecoverablePeerList;

		using ExpiredRecoverablePeer = PeerRoomManager::ExpiredRecoverablePeer;
		using ExpiredRecoverablePeerList = PeerRoomManager::ExpiredRecoverablePeerList;

	public:
		struct AuthenticatedIdentity
		{
		public:
			common::identity::AccountId accountId = 0;
			common::identity::PersistentPlayerId persistentPlayerId = 0;
			common::net::SessionToken sessionToken{};
			std::string_view nickname;
		};

		enum class JoinAuthenticatedPeerStatus
		{
			Joined,
			Recovered,
			ExistingPeer,
			Unauthenticated,
			Rejected,
		};

		struct JoinResult
		{
		public:
			bool shouldSendResponse = false;
			bool shouldBroadcastPlayerJoined = false;

			PlayerId playerId = 0;
			common::identity::PersistentPlayerId persistentPlayerId = 0;
			RoomId roomId = 0;
			common::game::SpawnPoint spawnPosition{};
		};

		struct JoinAuthenticatedPeerResult
		{
		public:
			JoinAuthenticatedPeerStatus status = JoinAuthenticatedPeerStatus::Unauthenticated;
			JoinResult joinResult{};
		};

		struct LeaveResult
		{
		public:
			bool shouldBroadcastPlayerLeft = false;

			PlayerId playerId = 0;
			common::identity::PersistentPlayerId persistentPlayerId = 0;
			RoomId roomId = 0;
		};

		struct RoomChangeResult
		{
		public:
			bool changed = false;

			PlayerId playerId = 0;
			common::identity::PersistentPlayerId persistentPlayerId = 0;
			RoomId previousRoomId = 0;
			RoomId nextRoomId = 0;
			common::game::SpawnPoint spawnPosition{};
		};

	public:
		PeerSessionService() = default;
		~PeerSessionService() noexcept = default;

		PeerSessionService(const PeerSessionService&) = delete;
		PeerSessionService& operator=(const PeerSessionService&) = delete;

		PeerSessionService(PeerSessionService&&) = delete;
		PeerSessionService& operator=(PeerSessionService&&) = delete;

	public:
		[[nodiscard]] JoinAuthenticatedPeerResult JoinAuthenticatedPeer(
			const EndpointKey& endpointKey,
			common::net::SessionToken sessionToken,
			RoomId initialRoomId,
			AuthenticatedAccountRegistry& authenticatedAccountRegistry,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			const common::game::GameRuleConfig& gameRuleConfig,
			TimePoint currentTime
		) const;
		[[nodiscard]] JoinResult JoinPeer(
			const EndpointKey& endpointKey,
			const AuthenticatedIdentity& authenticatedIdentity,
			RoomId initialRoomId,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			const common::game::GameRuleConfig& gameRuleConfig,
			TimePoint currentTime
		) const;
		[[nodiscard]] LeaveResult LeavePeer(const EndpointKey& endpointKey, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld) const;
		[[nodiscard]] RoomChangeResult ChangePeerRoom(
			const EndpointKey& endpointKey,
			RoomId nextRoomId,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			TimePoint currentTime
		) const;

		[[nodiscard]] RecoverablePeerList MarkTimedOutPeersRecoverable(
			TimePoint currentTime,
			Duration timeout,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld
		) const;
		[[nodiscard]] ExpiredRecoverablePeerList RemoveExpiredRecoverablePeers(
			TimePoint currentTime,
			Duration gracePeriod,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld
		) const;

	private:
		[[nodiscard]] JoinResult BuildCurrentJoinResult(const PeerState& peerState, const game::GameWorld& gameWorld) const noexcept;

		[[nodiscard]] game::PlayerState CreateInitialPlayerState(
			PlayerId playerId,
			const common::game::SpawnPoint& spawnPosition,
			const common::game::GameRuleConfig& gameRuleConfig
		) const noexcept;
	};
}
