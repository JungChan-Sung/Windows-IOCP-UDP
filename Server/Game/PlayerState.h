#pragma once

#include <cstdint>

#include <Common/Game/GameRules.h>
#include <Common/Game/InputFlags.h>
#include <Common/Game/WeaponRules.h>
#include <Common/Game/SimulationConstants.h>

namespace server::game
{
	struct PlayerState
	{
	public:
		std::uint32_t playerId = 0;
		float x = 0.0F;
		float y = 0.0F;
		float moveSpeed = common::game::defaultMoveSpeed;
		common::game::InputFlags inputFlags = common::game::InputFlags::None;
		common::game::WeaponType weaponType = common::game::WeaponType::Basic;

		int hp = common::game::defaultInitialPlayerHp;
		bool isDead = false;

		std::uint32_t killCount = 0;
		std::uint32_t deathCount = 0;

		float respawnRemainingSeconds = 0.0F;
		float invincibilityRemainingSeconds = 0.0F;
		float hitFlashRemainingSeconds = 0.0F;
		float fireCooldownRemainingSeconds = 0.0F;

		float lastMoveDirectionX = 1.0F;
		float lastMoveDirectionY = 0.0F;
	};
}