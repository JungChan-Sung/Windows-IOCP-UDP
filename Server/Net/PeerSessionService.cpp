#include "PeerSessionService.h"

#include <utility>

#include <Common/Game/SimulationConstants.h>
#include <Common/Game/InputFlags.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Time/TimeTypes.h>

namespace server::net
{
	PeerSessionService::JoinResult PeerSessionService::JoinPeer(const sockaddr_in& remoteAddress, const EndpointKey& endpointKey, const AuthenticatedIdentity& authenticatedIdentity, RoomId initialRoomId, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld, const game::GameSimulation& gameSimulation, const config::GameRuleConfig& gameRuleConfig, const config::ReliableUdpConfig& reliableUdpConfig, TimePoint currentTime) const
	{
		JoinResult joinResult{};
		joinResult.remoteAddress = remoteAddress;

		PeerState* existingPeerState = peerRoomManager.FindJoinedPeer(endpointKey);
		if (existingPeerState != nullptr)
		{
			existingPeerState->lastRecvTime = currentTime;

			const game::PlayerState* existingPlayerState = gameWorld.FindPlayer(existingPeerState->playerId);
			if (existingPlayerState != nullptr)
			{

				joinResult.shouldSendResponse = true;
				joinResult.shouldBroadcastPlayerJoined = false;
				joinResult.playerId = existingPeerState->playerId;
				joinResult.roomId = existingPeerState->roomId;
				joinResult.spawnPosition.x = existingPlayerState->x;
				joinResult.spawnPosition.y = existingPlayerState->y;
				return joinResult;
			}

			PlayerId removedPlayerId = 0;
			RoomId removedRoomId = 0;
			peerRoomManager.RemovePeer(endpointKey, removedPlayerId, removedRoomId);

			if (authenticatedIdentity.accountId <= 0 || authenticatedIdentity.nickname.empty())
			{
				return joinResult;
			}
		}

		const std::size_t spawnIndex = peerRoomManager.GetRoomMemberCount(initialRoomId);
		const game::GameSimulation::SpawnPosition spawnPosition = gameSimulation.GetSpawnPosition(initialRoomId, spawnIndex);

		const PlayerId playerId = gameWorld.AllocatePlayerId();

		PeerState& peerState = peerRoomManager.UpsertJoinedPeer(
			remoteAddress,
			endpointKey,
			playerId,
			initialRoomId,
			currentTime
		);
		peerState.accountId = authenticatedIdentity.accountId;
		peerState.nickname = authenticatedIdentity.nickname;
		peerState.lastInputSequence = 0;

		peerState.reliableSession.SetMaxPendingPacketCount(reliableUdpConfig.maxPendingPacketCount);
		peerState.reliableSession.SetMaxResendCount(reliableUdpConfig.maxResendCount);
		peerState.reliableSession.SetResendInterval(reliableUdpConfig.resendInterval);

		game::PlayerState playerState = CreateInitialPlayerState(playerId, spawnPosition, gameRuleConfig);
		gameWorld.UpsertPlayer(std::move(playerState));

		joinResult.shouldSendResponse = true;
		joinResult.shouldBroadcastPlayerJoined = true;
		joinResult.playerId = playerId;
		joinResult.roomId = initialRoomId;
		joinResult.spawnPosition = spawnPosition;
		return joinResult;
	}

	PeerSessionService::LeaveResult PeerSessionService::LeavePeer(const EndpointKey& endpointKey, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld) const
	{
		LeaveResult leaveResult{};

		PlayerId playerId = 0;
		RoomId roomId = 0;

		if (!peerRoomManager.RemovePeer(endpointKey, playerId, roomId))
		{
			return leaveResult;
		}

		gameWorld.RemovePlayer(playerId);

		leaveResult.shouldBroadcastPlayerLeft = true;
		leaveResult.playerId = playerId;
		leaveResult.roomId = roomId;
		return leaveResult;
	}

	PeerSessionService::RoomChangeResult PeerSessionService::ChangePeerRoom(const EndpointKey& endpointKey, RoomId nextRoomId, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld, const game::GameSimulation& gameSimulation, TimePoint currentTime) const
	{
		RoomChangeResult changeResult{};

		if (nextRoomId <= 0)
		{
			return changeResult;
		}

		PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);
		if (peerState == nullptr)
		{
			return changeResult;
		}

		game::PlayerState* playerState = gameWorld.FindPlayer(peerState->playerId);
		if (playerState == nullptr)
		{
			return changeResult;
		}

		if (playerState->isDead)
		{
			return changeResult;
		}

		if (peerState->roomId == nextRoomId)
		{
			return changeResult;
		}

		PeerRoomManager::RoomChangeResult roomChangeResult{};
		const bool changed = peerRoomManager.ChangePeerRoom(
			endpointKey,
			nextRoomId,
			currentTime,
			roomChangeResult
		);

		if (!changed)
		{
			return changeResult;
		}

		const std::size_t joinedRoomMemberCount = peerRoomManager.GetRoomMemberCount(nextRoomId);
		const std::size_t spawnIndex = (joinedRoomMemberCount > 0) ? joinedRoomMemberCount - 1 : 0;

		const game::GameSimulation::SpawnPosition spawnPosition = gameSimulation.GetSpawnPosition(nextRoomId, spawnIndex);

		playerState->x = spawnPosition.x;
		playerState->y = spawnPosition.y;
		playerState->inputFlags = common::game::InputFlags::None;

		changeResult.changed = true;
		changeResult.playerId = roomChangeResult.playerId;
		changeResult.previousRoomId = roomChangeResult.previousRoomId;
		changeResult.nextRoomId = roomChangeResult.nextRoomId;
		changeResult.remoteAddress = roomChangeResult.remoteAddress;
		changeResult.spawnPosition = spawnPosition;
		return changeResult;
	}

	game::PlayerState PeerSessionService::CreateInitialPlayerState(PlayerId playerId, const game::GameSimulation::SpawnPosition& spawnPosition, const config::GameRuleConfig& gameRuleConfig) const noexcept
	{
		game::PlayerState playerState{};
		playerState.playerId = playerId;
		playerState.x = spawnPosition.x;
		playerState.y = spawnPosition.y;
		playerState.moveSpeed = common::game::defaultMoveSpeed;
		playerState.inputFlags = common::game::InputFlags::None;
		playerState.weaponType = common::game::WeaponType::Basic;
		playerState.hp = gameRuleConfig.initialPlayerHp;
		playerState.isDead = false;
		playerState.respawnRemainingSeconds = 0.0F;
		playerState.invincibilityRemainingSeconds = 0.0F;
		playerState.hitFlashRemainingSeconds = 0.0F;
		playerState.killCount = 0;
		playerState.deathCount = 0;
		playerState.fireCooldownRemainingSeconds = 0.0F;
		playerState.lastMoveDirectionX = 1.0F;
		playerState.lastMoveDirectionY = 0.0F;
		return playerState;
	}
}