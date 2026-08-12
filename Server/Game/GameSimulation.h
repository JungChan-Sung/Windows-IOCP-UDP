#pragma once

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Net/Endpoint.h>
#include <Common/Packet/Game/GamePacket.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Game/BulletState.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/KillEvent.h>
#include <Server/Game/PlayerState.h>
#include <Server/Net/PeerState.h>

namespace server::game
{
	class GameSimulation
	{
	public:
		struct SpawnPosition
		{
		public:
			float x = 0.0F;
			float y = 0.0F;
		};

	public:
		using PlayerId = common::game::PlayerId;
		using BulletId = common::game::BulletId;
		using RoomId = common::game::RoomId;
		using EndpointKey = common::net::EndpointKey;

		using PeerTable = std::unordered_map<EndpointKey, net::PeerState, common::net::EndpointKeyHasher>;

	public:
		GameSimulation() = default;
		~GameSimulation() noexcept = default;

		GameSimulation(const GameSimulation&) = delete;
		GameSimulation& operator=(const GameSimulation&) = delete;

		GameSimulation(GameSimulation&&) = delete;
		GameSimulation& operator=(GameSimulation&&) = delete;

	public:
		void UpdatePlayers(float deltaTime, const PeerTable& peerTable, GameWorld& gameWorld) const;
		[[nodiscard]] KillEventList UpdateBullets(
			float deltaTime, 
			const PeerTable& peerTable,	
			GameWorld& gameWorld, 
			const config::GameRuleConfig& gameRuleConfig
		) const;
		void UpdateRespawns(
			float deltaTime,
			const PeerTable& peerTable, 
			GameWorld& gameWorld,
			const config::GameRuleConfig& gameRuleConfig
		) const;
		void UpdatePlayerTimers(float deltaTime, GameWorld& gameWorld) const;

		[[nodiscard]] BulletState CreateBullet(
			BulletId bulletId,
			PlayerId ownerPlayerId,
			std::int64_t ownerPersistentPlayerId,
			RoomId roomId,
			const PlayerState& playerState,
			float directionX,
			float directionY, 
			const config::WeaponRuleConfig& weaponRuleConfig
		) const;

		void SpawnImpactEffect(
			float x, 
			float y, 
			RoomId roomId, 
			common::packet::EffectType effectType, 
			GameWorld& gameWorld
		) const;

		[[nodiscard]] SpawnPosition GetSpawnPosition(RoomId roomId, std::size_t spawnIndex) const;

	private:
		void RespawnPlayer(
			PlayerId playerId, 
			RoomId roomId,
			GameWorld& gameWorld,
			const config::GameRuleConfig& gameRuleConfig
		) const;

		[[nodiscard]] bool IsBulletCollidingWithPlayer(const BulletState& bulletState, const PlayerState& playerState) const noexcept;

		void UpdateTimer(float deltaTime, float& remainingSeconds) const noexcept;
	};
}
