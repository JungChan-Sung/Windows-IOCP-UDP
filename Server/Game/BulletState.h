#pragma once

#include <cstdint>

#include <Common/Game/GameTypes.h>
#include <Common/Game/WeaponRules.h>

namespace server::game
{
	struct BulletState
	{
	public:
		common::game::BulletId bulletId = 0;
		common::game::PlayerId ownerPlayerId = 0;
		std::int64_t ownerPersistentPlayerId = 0;
		common::game::RoomId roomId = 0;

		float x = 0.0F;
		float y = 0.0F;
		float velocityX = 0.0F;
		float velocityY = 0.0F;
		float remainingLifeSeconds = 0.0F;

		int damage = common::game::defaultBasicWeaponRule.bulletDamage;
		float radius = common::game::defaultBasicWeaponRule.bulletRadius;
	};
}