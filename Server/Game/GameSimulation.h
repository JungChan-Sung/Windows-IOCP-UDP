#pragma once

#include <cstddef>
#include <span>

#include <Common/Game/EffectType.h>
#include <Common/Game/GameRules.h>
#include <Common/Game/GameTypes.h>

#include <Server/Game/BulletState.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/KillEvent.h>
#include <Server/Game/PlayerSimulationContext.h>
#include <Server/Game/PlayerState.h>

namespace server::game
{
	class GameSimulation
	{
	public:
		using PlayerId = common::game::PlayerId;
		using BulletId = common::game::BulletId;
		using RoomId = common::game::RoomId;

	public:
		GameSimulation() = default;
		~GameSimulation() noexcept = default;

		GameSimulation(const GameSimulation&) = delete;
		GameSimulation& operator=(const GameSimulation&) = delete;

		GameSimulation(GameSimulation&&) = delete;
		GameSimulation& operator=(GameSimulation&&) = delete;

	public:
		void UpdatePlayers(float deltaTime, std::span<const PlayerSimulationContext> playerContextList, GameWorld& gameWorld) const;
		[[nodiscard]] KillEventList UpdateBullets(
			float deltaTime, 
			std::span<const PlayerSimulationContext> playerContextList,
			GameWorld& gameWorld, 
			const common::game::GameRuleConfig& gameRuleConfig
		) const;
		void UpdateRespawns(
			float deltaTime,
			std::span<const PlayerSimulationContext> playerContextList,
			GameWorld& gameWorld,
			const common::game::GameRuleConfig& gameRuleConfig
		) const;
		void UpdatePlayerTimers(float deltaTime, GameWorld& gameWorld) const;

	private:
		void RespawnPlayer(
			PlayerId playerId, 
			RoomId roomId,
			GameWorld& gameWorld,
			const common::game::GameRuleConfig& gameRuleConfig
		) const;

		void SpawnImpactEffect(float x, float y, RoomId roomId, common::game::EffectType effectType, GameWorld& gameWorld) const;

		[[nodiscard]] bool IsBulletCollidingWithPlayer(const BulletState& bulletState, const PlayerState& playerState) const noexcept;

		void UpdateTimer(float deltaTime, float& remainingSeconds) const noexcept;
	};
}
