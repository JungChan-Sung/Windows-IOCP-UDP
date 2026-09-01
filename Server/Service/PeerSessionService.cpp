#include "PeerSessionService.h"

#include <utility>

#include <Common/Game/SimulationConstants.h>
#include <Common/Game/InputFlags.h>
#include <Common/Game/WeaponRules.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Service/AuthenticatedAccountRegistry.h>

namespace server::service
{
	PeerSessionService::JoinAuthenticatedPeerResult PeerSessionService::JoinAuthenticatedPeer(const EndpointKey& endpointKey, common::net::SessionToken sessionToken, RoomId initialRoomId, AuthenticatedAccountRegistry& authenticatedAccountRegistry, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld, const common::game::GameRuleConfig& gameRuleConfig, TimePoint currentTime) const
	{
		AuthenticatedIdentity authenticatedIdentity{};

		const PeerState* existingPeerState = peerRoomManager.FindJoinedPeer(endpointKey);
		if (existingPeerState != nullptr)
		{
			if (existingPeerState->connectionState == PeerConnectionState::Recoverable)
			{
				if (existingPeerState->sessionToken != sessionToken)
				{
					return JoinAuthenticatedPeerResult{
						.status = JoinAuthenticatedPeerStatus::Rejected,
					};
				}

				const JoinResult joinResult = BuildCurrentJoinResult(*existingPeerState, gameWorld);
				if (!joinResult.shouldSendResponse)
				{
					return JoinAuthenticatedPeerResult{
						.status = JoinAuthenticatedPeerStatus::Rejected,
					};
				}

				if (!peerRoomManager.RebindRecoverablePeer(endpointKey, endpointKey, currentTime))
				{
					return JoinAuthenticatedPeerResult{
						.status = JoinAuthenticatedPeerStatus::Rejected,
					};
				}

				return JoinAuthenticatedPeerResult{
					.status = JoinAuthenticatedPeerStatus::Recovered,
					.joinResult = joinResult,
				};
			}

			// JoinResponse 유실 등에 의한 기존 Connected peer의 재요청.
			// 요청 token은 JoinPeer()에서 기존 PeerState token과 비교한다.
			authenticatedIdentity.accountId = existingPeerState->accountId;
			authenticatedIdentity.persistentPlayerId = existingPeerState->persistentPlayerId;
			authenticatedIdentity.sessionToken = sessionToken;
			authenticatedIdentity.nickname = existingPeerState->nickname;
		}
		else
		{
			const PeerState* recoverablePeerState = peerRoomManager.FindRecoverablePeerBySessionToken(sessionToken);
			if (recoverablePeerState != nullptr)
			{
				const JoinResult joinResult = BuildCurrentJoinResult(*recoverablePeerState, gameWorld);
				if (!joinResult.shouldSendResponse)
				{
					return JoinAuthenticatedPeerResult{
						.status = JoinAuthenticatedPeerStatus::Rejected,
					};
				}

				const EndpointKey previousEndpointKey = recoverablePeerState->endpointKey;
				if (!peerRoomManager.RebindRecoverablePeer(previousEndpointKey, endpointKey, currentTime))
				{
					return JoinAuthenticatedPeerResult{
						.status = JoinAuthenticatedPeerStatus::Rejected,
					};
				}

				return JoinAuthenticatedPeerResult{
					.status = JoinAuthenticatedPeerStatus::Recovered,
					.joinResult = joinResult,
				};
			}

			const AuthenticatedAccount* authenticatedAccount = authenticatedAccountRegistry.Find(endpointKey, sessionToken);
			if (authenticatedAccount == nullptr)
			{
				return JoinAuthenticatedPeerResult{
					.status = JoinAuthenticatedPeerStatus::Unauthenticated,
				};
			}

			authenticatedIdentity.accountId = authenticatedAccount->accountId;
			authenticatedIdentity.persistentPlayerId = authenticatedAccount->persistentPlayerId;
			authenticatedIdentity.sessionToken = authenticatedAccount->sessionToken;
			authenticatedIdentity.nickname = authenticatedAccount->nickname;
		}

		JoinResult joinResult = JoinPeer(
			endpointKey,
			authenticatedIdentity,
			initialRoomId,
			peerRoomManager,
			gameWorld,
			gameRuleConfig,
			currentTime
		);

		if (!joinResult.shouldSendResponse)
		{
			return JoinAuthenticatedPeerResult{
				.status = JoinAuthenticatedPeerStatus::Rejected,
				.joinResult = joinResult,
			};
		}

		if (!joinResult.shouldBroadcastPlayerJoined)
		{
			return JoinAuthenticatedPeerResult{
				.status = JoinAuthenticatedPeerStatus::ExistingPeer,
				.joinResult = joinResult,
			};
		}

		static_cast<void>(authenticatedAccountRegistry.Remove(endpointKey));

		return JoinAuthenticatedPeerResult{
			.status = JoinAuthenticatedPeerStatus::Joined,
			.joinResult = joinResult,
		};
	}

	PeerSessionService::JoinResult PeerSessionService::JoinPeer(const EndpointKey& endpointKey, const AuthenticatedIdentity& authenticatedIdentity, RoomId initialRoomId, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld, const common::game::GameRuleConfig& gameRuleConfig, TimePoint currentTime) const
	{
		JoinResult joinResult{};

		if (authenticatedIdentity.accountId <= 0 
			|| authenticatedIdentity.persistentPlayerId <= 0 
			|| !common::net::IsValidSessionToken(authenticatedIdentity.sessionToken) 
			|| authenticatedIdentity.nickname.empty())
		{
			return joinResult;
		}

		PeerState* existingPeerState = peerRoomManager.FindJoinedPeer(endpointKey);
		if (existingPeerState != nullptr)
		{
			if (existingPeerState->accountId != authenticatedIdentity.accountId 
				|| existingPeerState->persistentPlayerId != authenticatedIdentity.persistentPlayerId
				|| existingPeerState->sessionToken != authenticatedIdentity.sessionToken)
			{
				return joinResult;
			}

			existingPeerState->lastRecvTime = currentTime;

			joinResult = BuildCurrentJoinResult(*existingPeerState, gameWorld);
			if (joinResult.shouldSendResponse)
			{
				return joinResult;
			}

			PlayerId removedPlayerId = 0;
			RoomId removedRoomId = 0;
			peerRoomManager.RemovePeer(endpointKey, removedPlayerId, removedRoomId);
		}

		const std::size_t spawnIndex = peerRoomManager.GetRoomMemberCount(initialRoomId);
		const common::game::SpawnPoint spawnPosition = common::game::GetSpawnPointForRoom(initialRoomId, spawnIndex);

		const PlayerId playerId = gameWorld.AllocatePlayerId();

		PeerState& peerState = peerRoomManager.UpsertJoinedPeer(endpointKey, playerId, initialRoomId, currentTime);
		peerState.accountId = authenticatedIdentity.accountId;
		peerState.persistentPlayerId = authenticatedIdentity.persistentPlayerId;
		peerState.sessionToken = authenticatedIdentity.sessionToken;
		peerState.nickname = authenticatedIdentity.nickname;
		peerState.lastAcceptedInputSequence = 0;
		peerState.lastProcessedInputSequence = 0;

		game::PlayerState playerState = CreateInitialPlayerState(playerId, spawnPosition, gameRuleConfig);
		gameWorld.UpsertPlayer(std::move(playerState));

		joinResult.shouldSendResponse = true;
		joinResult.shouldBroadcastPlayerJoined = true;
		joinResult.playerId = playerId;
		joinResult.persistentPlayerId = authenticatedIdentity.persistentPlayerId;
		joinResult.roomId = initialRoomId;
		joinResult.spawnPosition = spawnPosition;
		return joinResult;
	}

	PeerSessionService::LeaveResult PeerSessionService::LeavePeer(const EndpointKey& endpointKey, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld) const
	{
		LeaveResult leaveResult{};

		const PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);
		if (peerState == nullptr)
		{
			return leaveResult;
		}

		const common::identity::PersistentPlayerId persistentPlayerId = peerState->persistentPlayerId;

		PlayerId playerId = 0;
		RoomId roomId = 0;
		if (!peerRoomManager.RemovePeer(endpointKey, playerId, roomId))
		{
			return leaveResult;
		}

		gameWorld.RemovePlayer(playerId);

		if (peerRoomManager.GetRoomMemberCount(roomId) == 0)
		{
			gameWorld.ClearRoomTransientState(roomId);
		}

		leaveResult.shouldBroadcastPlayerLeft = true;
		leaveResult.playerId = playerId;
		leaveResult.persistentPlayerId = persistentPlayerId;
		leaveResult.roomId = roomId;
		return leaveResult;
	}

	PeerSessionService::RoomChangeResult PeerSessionService::ChangePeerRoom(const EndpointKey& endpointKey, RoomId nextRoomId, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld, TimePoint currentTime) const
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

		if (peerRoomManager.GetRoomMemberCount(roomChangeResult.previousRoomId) == 0)
		{
			gameWorld.ClearRoomTransientState(roomChangeResult.previousRoomId);
		}

		const std::size_t joinedRoomMemberCount = peerRoomManager.GetRoomMemberCount(nextRoomId);
		const std::size_t spawnIndex = (joinedRoomMemberCount > 0) ? joinedRoomMemberCount - 1 : 0;

		const common::game::SpawnPoint spawnPosition = common::game::GetSpawnPointForRoom(nextRoomId, spawnIndex);

		playerState->x = spawnPosition.x;
		playerState->y = spawnPosition.y;
		playerState->inputFlags = common::game::InputFlags::None;

		changeResult.changed = true;
		changeResult.playerId = roomChangeResult.playerId;
		changeResult.persistentPlayerId = peerState->persistentPlayerId;
		changeResult.previousRoomId = roomChangeResult.previousRoomId;
		changeResult.nextRoomId = roomChangeResult.nextRoomId;
		changeResult.spawnPosition = spawnPosition;
		return changeResult;
	}

	PeerSessionService::RecoverablePeerList PeerSessionService::MarkTimedOutPeersRecoverable(TimePoint currentTime, Duration timeout, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld) const
	{
		RecoverablePeerList recoverablePeerList = peerRoomManager.MarkTimedOutPeersRecoverable(currentTime, timeout);
		for (const RecoverablePeer& recoverablePeer : recoverablePeerList)
		{
			game::PlayerState* playerState = gameWorld.FindPlayer(recoverablePeer.playerId);
			if (playerState == nullptr)
			{
				continue;
			}

			playerState->inputFlags = common::game::InputFlags::None;
		}

		return recoverablePeerList;
	}

	PeerSessionService::ExpiredRecoverablePeerList PeerSessionService::RemoveExpiredRecoverablePeers(TimePoint currentTime, Duration gracePeriod, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld) const
	{
		ExpiredRecoverablePeerList expiredPeerList = peerRoomManager.RemoveExpiredRecoverablePeers(currentTime, gracePeriod);
		for (const ExpiredRecoverablePeer& expiredPeer : expiredPeerList)
		{
			gameWorld.RemovePlayer(expiredPeer.playerId);

			if (peerRoomManager.GetRoomMemberCount(expiredPeer.roomId) == 0)
			{
				gameWorld.ClearRoomTransientState(expiredPeer.roomId);
			}
		}

		return expiredPeerList;
	}

	PeerSessionService::JoinResult PeerSessionService::BuildCurrentJoinResult(const PeerState& peerState, const game::GameWorld& gameWorld) const noexcept
	{
		JoinResult joinResult{};

		const game::PlayerState* playerState = gameWorld.FindPlayer(peerState.playerId);
		if (playerState == nullptr)
		{
			return joinResult;
		}

		joinResult.shouldSendResponse = true;
		joinResult.shouldBroadcastPlayerJoined = false;
		joinResult.playerId = peerState.playerId;
		joinResult.persistentPlayerId = peerState.persistentPlayerId;
		joinResult.roomId = peerState.roomId;
		joinResult.spawnPosition.x = playerState->x;
		joinResult.spawnPosition.y = playerState->y;

		return joinResult;
	}

	game::PlayerState PeerSessionService::CreateInitialPlayerState(PlayerId playerId, const common::game::SpawnPoint& spawnPosition, const common::game::GameRuleConfig& gameRuleConfig) const noexcept
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