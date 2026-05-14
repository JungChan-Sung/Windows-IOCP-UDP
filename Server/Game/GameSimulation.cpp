#include "GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <Common/Game/Movement.h>
#include <Common/Game/RoomLayout.h>
#include <Common/Game/SimulationConstants.h>
#include <Common/Game/WorldCollision.h>
#include <Common/Game/GameRules.h>
#include <Common/Game/WeaponRules.h>

namespace
{
	[[nodiscard]] const common::game::WeaponRule& GetWeaponRule(
		common::game::WeaponType weaponType,
		const server::config::WeaponRuleConfig& weaponRuleConfig
	) noexcept
	{
		switch (weaponType)
		{
		case common::game::WeaponType::Basic:
			return weaponRuleConfig.basicWeaponRule;

		default:
			return weaponRuleConfig.basicWeaponRule;
		}
	}
}

namespace server::game
{
	void GameSimulation::UpdatePlayers(float deltaTime, const PeerTable& peerTable, GameWorld& gameWorld) const
	{
		for (const auto& [_, peerState] : peerTable)
		{
			if (!peerState.isJoined)
			{
				continue;
			}

			PlayerState* playerState = gameWorld.FindPlayer(peerState.playerId);
			if (playerState == nullptr)
			{
				continue;
			}

			if (playerState->isDead)
			{
				continue;
			}

			common::game::MovePlayerWithWallCollision(
				playerState->x,
				playerState->y,
				playerState->inputFlags,
				deltaTime,
				playerState->moveSpeed,
				common::game::playerHalfExtent,
				common::game::defaultWorldBounds,
				common::game::GetWallRectListForRoom(peerState.roomId)
			);
		}
	}

	void server::game::GameSimulation::UpdateBullets(float deltaTime, const PeerTable& peerTable, GameWorld& gameWorld, const config::GameRuleConfig& gameRuleConfig) const
	{
		GameWorld::BulletStateList& bulletStateList = gameWorld.GetBulletStateList();

		for (std::size_t bulletIndex = 0; bulletIndex < bulletStateList.size();)
		{
			BulletState& bulletState = bulletStateList[bulletIndex];

			const float previousX = bulletState.x;
			const float previousY = bulletState.y;

			bulletState.x += bulletState.velocityX * deltaTime;
			bulletState.y += bulletState.velocityY * deltaTime;
			bulletState.remainingLifeSeconds -= deltaTime;

			bool shouldEraseBullet = bulletState.remainingLifeSeconds <= 0.0F;

			const auto wallRectList = common::game::GetWallRectListForRoom(bulletState.roomId);

			if (!shouldEraseBullet)
			{
				const bool isOutsideWorldBounds = common::game::IsCircleOutsideWorldBounds(
					bulletState.x,
					bulletState.y,
					bulletState.radius,
					common::game::defaultWorldBounds
				);

				if (isOutsideWorldBounds)
				{
					float impactX = bulletState.x;
					float impactY = bulletState.y;

					common::game::FindCircleImpactPosition(
						previousX,
						previousY,
						bulletState.x,
						bulletState.y,
						bulletState.radius,
						common::game::defaultWorldBounds,
						wallRectList,
						impactX,
						impactY
					);

					SpawnImpactEffect(
						impactX,
						impactY,
						bulletState.roomId,
						common::packet::EffectType::Impact,
						gameWorld
					);

					shouldEraseBullet = true;
				}
			}

			if (!shouldEraseBullet)
			{
				const bool isCollidingWithWall = common::game::IsCircleCollidingWithAnyWall(
					bulletState.x,
					bulletState.y,
					bulletState.radius,
					wallRectList
				);

				if (isCollidingWithWall)
				{
					float impactX = bulletState.x;
					float impactY = bulletState.y;

					common::game::FindCircleImpactPosition(
						previousX,
						previousY,
						bulletState.x,
						bulletState.y,
						bulletState.radius,
						common::game::defaultWorldBounds,
						wallRectList,
						impactX,
						impactY
					);

					SpawnImpactEffect(
						impactX,
						impactY,
						bulletState.roomId,
						common::packet::EffectType::Impact,
						gameWorld
					);

					shouldEraseBullet = true;
				}
			}

			if (!shouldEraseBullet)
			{
				for (const auto& [_, peerState] : peerTable)
				{
					if (!peerState.isJoined)
					{
						continue;
					}

					if (peerState.playerId == bulletState.ownerPlayerId)
					{
						continue;
					}

					if (peerState.roomId != bulletState.roomId)
					{
						continue;
					}

					PlayerState* playerState = gameWorld.FindPlayer(peerState.playerId);
					if (playerState == nullptr)
					{
						continue;
					}

					if (playerState->isDead)
					{
						continue;
					}

					if (playerState->invincibilityRemainingSeconds > 0.0F)
					{
						continue;
					}

					if (!IsBulletCollidingWithPlayer(bulletState, *playerState))
					{
						continue;
					}

					playerState->hp = std::max(0, playerState->hp - bulletState.damage);
					playerState->hitFlashRemainingSeconds = gameRuleConfig.hitFlashDurationSeconds;

					SpawnImpactEffect(
						bulletState.x,
						bulletState.y,
						bulletState.roomId,
						common::packet::EffectType::Impact,
						gameWorld
					);

					if (playerState->hp <= 0)
					{
						playerState->isDead = true;
						playerState->respawnRemainingSeconds = gameRuleConfig.respawnDelaySeconds;
						playerState->invincibilityRemainingSeconds = 0.0F;
						playerState->inputFlags = common::game::InputFlags::None;
						++playerState->deathCount;

						PlayerState* ownerPlayerState = gameWorld.FindPlayer(bulletState.ownerPlayerId);
						if (ownerPlayerState != nullptr)
						{
							++ownerPlayerState->killCount;
						}
					}

					shouldEraseBullet = true;
					break;
				}
			}

			if (!shouldEraseBullet)
			{
				++bulletIndex;
				continue;
			}

			gameWorld.RemoveBulletAt(bulletIndex);
		}
	}

	void server::game::GameSimulation::UpdateRespawns(float deltaTime, const PeerTable& peerTable, GameWorld& gameWorld, const config::GameRuleConfig& gameRuleConfig) const
	{
		GameWorld::PlayerTable& playerTable = gameWorld.GetPlayerTable();

		for (auto& [playerId, playerState] : playerTable)
		{
			if (!playerState.isDead)
			{
				continue;
			}

			playerState.respawnRemainingSeconds -= deltaTime;
			if (playerState.respawnRemainingSeconds > 0.0F)
			{
				continue;
			}

			const auto peerIterator = std::find_if(
				peerTable.begin(),
				peerTable.end(),
				[playerId](const auto& pair)
				{
					return pair.second.playerId == playerId && pair.second.isJoined;
				}
			);

			if (peerIterator == peerTable.end())
			{
				continue;
			}

			RespawnPlayer(
				playerId,
				peerIterator->second.roomId,
				gameWorld, 
				gameRuleConfig
			);
		}
	}

	void GameSimulation::UpdatePlayerTimers(float deltaTime, GameWorld& gameWorld) const
	{
		for (auto& [_, playerState] : gameWorld.GetPlayerTable())
		{
			UpdateTimer(deltaTime, playerState.invincibilityRemainingSeconds);
			UpdateTimer(deltaTime, playerState.hitFlashRemainingSeconds);
			UpdateTimer(deltaTime, playerState.fireCooldownRemainingSeconds);
		}
	}

	BulletState server::game::GameSimulation::CreateBullet(BulletId bulletId, PlayerId ownerPlayerId, RoomId roomId, const PlayerState& playerState, float directionX, float directionY, const config::WeaponRuleConfig& weaponRuleConfig) const
	{
		const common::game::WeaponRule& weaponRule = GetWeaponRule(playerState.weaponType, weaponRuleConfig);

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

	void GameSimulation::SpawnImpactEffect(float x, float y, RoomId roomId, common::packet::EffectType effectType, GameWorld& gameWorld) const
	{
		game::ImpactEffectState impactEffectState{};
		impactEffectState.roomId = roomId;
		impactEffectState.effectType = effectType;
		impactEffectState.x = x;
		impactEffectState.y = y;

		gameWorld.AddPendingImpactEffect(std::move(impactEffectState));
	}

	GameSimulation::SpawnPosition GameSimulation::GetSpawnPosition(RoomId roomId, std::size_t spawnIndex) const
	{
		const std::span<const common::game::SpawnPoint> spawnPointList = common::game::GetSpawnPointListForRoom(roomId);
		if (!spawnPointList.empty())
		{
			const common::game::SpawnPoint& spawnPoint = spawnPointList[spawnIndex % spawnPointList.size()];

			SpawnPosition spawnPosition{};
			spawnPosition.x = spawnPoint.x;
			spawnPosition.y = spawnPoint.y;
			return spawnPosition;
		}

		SpawnPosition fallbackSpawnPosition{};
		fallbackSpawnPosition.x = 100.0F;
		fallbackSpawnPosition.y = 100.0F;
		return fallbackSpawnPosition;
	}

	void server::game::GameSimulation::RespawnPlayer(PlayerId playerId, RoomId roomId, GameWorld& gameWorld, const config::GameRuleConfig& gameRuleConfig) const
	{
		PlayerState* playerState = gameWorld.FindPlayer(playerId);
		if (playerState == nullptr)
		{
			return;
		}

		const SpawnPosition spawnPosition = GetSpawnPosition(roomId, 0);

		playerState->x = spawnPosition.x;
		playerState->y = spawnPosition.y;
		playerState->hp = gameRuleConfig.initialPlayerHp;
		playerState->isDead = false;
		playerState->respawnRemainingSeconds = 0.0F;
		playerState->invincibilityRemainingSeconds = gameRuleConfig.respawnInvincibilitySeconds;
		playerState->hitFlashRemainingSeconds = 0.0F;
		playerState->fireCooldownRemainingSeconds = 0.0F;
		playerState->inputFlags = common::game::InputFlags::None;
		playerState->lastMoveDirectionX = 1.0F;
		playerState->lastMoveDirectionY = 0.0F;

		SpawnImpactEffect(
			playerState->x,
			playerState->y,
			roomId,
			common::packet::EffectType::Spawn,
			gameWorld
		);
	}

	bool GameSimulation::IsBulletCollidingWithPlayer(const BulletState& bulletState, const PlayerState& playerState) const noexcept
	{
		const float deltaX = bulletState.x - playerState.x;
		const float deltaY = bulletState.y - playerState.y;
		const float distanceSquared = (deltaX * deltaX) + (deltaY * deltaY);

		const float collisionRadius = common::game::playerHalfExtent + bulletState.radius;
		return distanceSquared <= (collisionRadius * collisionRadius);
	}

	void GameSimulation::UpdateTimer(float deltaTime, float& remainingSeconds) const noexcept
	{
		if (remainingSeconds <= 0.0F)
		{
			return;
		}

		remainingSeconds -= deltaTime;
		if (remainingSeconds < 0.0F)
		{
			remainingSeconds = 0.0F;
		}
	}
}