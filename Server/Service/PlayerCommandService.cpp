#include "PlayerCommandService.h"

#include <utility>

#include <Common/Game/GameRules.h>
#include <Common/Game/Movement.h>
#include <Common/Net/SequenceNumber.h>

#include <Server/Game/BulletFactory.h>

namespace
{
	void UpdateLastMoveDirection(server::game::PlayerState& playerState, common::game::InputFlags inputFlags) noexcept
	{
		const common::game::MoveDirection direction = common::game::BuildNormalizedMoveDirection(inputFlags);
		if (direction.x == 0.0F && direction.y == 0.0F)
		{
			return;
		}

		playerState.lastMoveDirectionX = direction.x;
		playerState.lastMoveDirectionY = direction.y;
	}
}

namespace server::service
{
	bool PlayerCommandService::ApplyInputCommand(const EndpointKey& endpointKey, std::uint32_t inputSequence, common::game::InputFlags inputFlags, PeerRoomManager & peerRoomManager, game::GameWorld& gameWorld, TimePoint currentTime) const
	{
		PeerPlayerView peerPlayerView = FindJoinedPeerPlayer(endpointKey, peerRoomManager, gameWorld);
		if (peerPlayerView.peerState == nullptr || peerPlayerView.playerState == nullptr)
		{
			return false;
		}

		PeerState& peerState = *peerPlayerView.peerState;
		game::PlayerState& playerState = *peerPlayerView.playerState;

		if (playerState.isDead)
		{
			return false;
		}

		if (!common::net::IsSequenceNewer(inputSequence, peerState.lastAcceptedInputSequence))
		{
			return false;
		}

		peerState.lastAcceptedInputSequence = inputSequence;
		peerState.lastRecvTime = currentTime;
		playerState.inputFlags = inputFlags;

		UpdateLastMoveDirection(playerState, inputFlags);

		return true;
	}

	bool PlayerCommandService::FireBullet(const EndpointKey& endpointKey, PeerRoomManager & peerRoomManager, game::GameWorld& gameWorld, const common::game::WeaponRuleConfig& weaponRuleConfig, TimePoint currentTime) const
	{
		PeerPlayerView peerPlayerView = FindJoinedPeerPlayer(endpointKey, peerRoomManager, gameWorld);
		if (peerPlayerView.peerState == nullptr || peerPlayerView.playerState == nullptr)
		{
			return false;
		}

		PeerState& peerState = *peerPlayerView.peerState;
		game::PlayerState& playerState = *peerPlayerView.playerState;

		if (playerState.isDead)
		{
			return false;
		}

		peerState.lastRecvTime = currentTime;

		if (playerState.fireCooldownRemainingSeconds > 0.0F)
		{
			return false;
		}

		const common::game::WeaponRule& weaponRule = common::game::GetWeaponRule(playerState.weaponType, weaponRuleConfig);
		game::BulletState bulletState = game::CreateBulletState(
			gameWorld.AllocateBulletId(),
			peerState.playerId,
			peerState.persistentPlayerId,
			peerState.roomId,
			playerState,
			playerState.lastMoveDirectionX,
			playerState.lastMoveDirectionY,
			weaponRule
		);

		gameWorld.AddBullet(std::move(bulletState));

		playerState.fireCooldownRemainingSeconds = weaponRule.fireCooldownSeconds;
		return true;
	}

	PlayerCommandService::PeerPlayerView PlayerCommandService::FindJoinedPeerPlayer(const EndpointKey& endpointKey, PeerRoomManager& peerRoomManager, game::GameWorld& gameWorld) const
	{
		PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);
		if (peerState == nullptr)
		{
			return {};
		}

		game::PlayerState* playerState = gameWorld.FindPlayer(peerState->playerId);
		if (playerState == nullptr)
		{
			return {};
		}

		PeerPlayerView peerPlayerView;
		peerPlayerView.peerState = peerState;
		peerPlayerView.playerState = playerState;
		return peerPlayerView;
	}
}