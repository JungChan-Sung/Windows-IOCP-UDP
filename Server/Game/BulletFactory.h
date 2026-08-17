#pragma once

#include <cmath>

#include <Common/Game/GameTypes.h>
#include <Common/Game/WeaponRules.h>
#include <Common/Identity/IdentityTypes.h>

#include <Server/Game/BulletState.h>
#include <Server/Game/PlayerState.h>

namespace server::game
{
	[[nodiscard]] inline BulletState CreateBulletState(
		common::game::BulletId bulletId,
		common::game::PlayerId ownerPlayerId,
		common::identity::PersistentPlayerId ownerPersistentPlayerId,
		common::game::RoomId roomId,
		const PlayerState& playerState,
		float directionX,
		float directionY,
		const common::game::WeaponRule& weaponRule
	)
	{
		const float length = std::sqrt((directionX * directionX) + (directionY * directionY));

		float normalizedDirectionX = 1.0F;
		float normalizedDirectionY = 0.0F;

		if (length > 0.0F)
		{
			normalizedDirectionX = directionX / length;
			normalizedDirectionY = directionY / length;
		}

		BulletState bulletState{};
		bulletState.bulletId = bulletId;
		bulletState.ownerPlayerId = ownerPlayerId;
		bulletState.ownerPersistentPlayerId = ownerPersistentPlayerId;
		bulletState.roomId = roomId;
		bulletState.x = playerState.x;
		bulletState.y = playerState.y;
		bulletState.velocityX = normalizedDirectionX * weaponRule.bulletSpeed;
		bulletState.velocityY = normalizedDirectionY * weaponRule.bulletSpeed;
		bulletState.remainingLifeSeconds = weaponRule.bulletLifeSeconds;
		bulletState.damage = weaponRule.bulletDamage;
		bulletState.radius = weaponRule.bulletRadius;
		return bulletState;
	}
}