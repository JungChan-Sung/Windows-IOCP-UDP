#include "GameSimulationTests.h"

#include <cmath>
#include <cstdint>

#include <Common/Game/GameRules.h>
#include <Common/Game/InputFlags.h>
#include <Common/Game/WeaponRules.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Game/BulletState.h>
#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/PlayerState.h>
#include <Server/Net/PeerState.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using PeerTable = server::game::GameSimulation::PeerTable;

	[[nodiscard]] bool IsNearlyEqual(float lhs, float rhs, float epsilon = 0.001F) noexcept
	{
		return std::fabs(lhs - rhs) <= epsilon;
	}

	[[nodiscard]] common::net::EndpointKey MakeEndpointKey(std::uint32_t index) noexcept
	{
		common::net::EndpointKey endpointKey{};
		endpointKey.address = index;
		endpointKey.port = static_cast<std::uint16_t>(10000 + index);
		return endpointKey;
	}

	void AddJoinedPeer(PeerTable& peerTable, common::game::PlayerId playerId, common::game::RoomId roomId)
	{
		const common::net::EndpointKey endpointKey = MakeEndpointKey(playerId);

		server::net::PeerState peerState{};
		peerState.endpointKey = endpointKey;
		peerState.playerId = playerId;
		peerState.roomId = roomId;
		peerState.isJoined = true;

		peerTable.insert_or_assign(endpointKey, peerState);
	}

	[[nodiscard]] server::game::PlayerState MakePlayer(common::game::PlayerId playerId, float x, float y)
	{
		server::game::PlayerState playerState{};
		playerState.playerId = playerId;
		playerState.x = x;
		playerState.y = y;
		playerState.hp = common::game::defaultInitialPlayerHp;
		playerState.isDead = false;
		playerState.weaponType = common::game::WeaponType::Basic;
		playerState.lastMoveDirectionX = 1.0F;
		playerState.lastMoveDirectionY = 0.0F;
		return playerState;
	}

	void RunCreateBulletDirectionTest(tests::DebugTestResult& result)
	{
		server::game::GameSimulation simulation;
		server::config::WeaponRuleConfig weaponRuleConfig{};

		server::game::PlayerState ownerPlayer{};
		ownerPlayer.playerId = 1;
		ownerPlayer.x = 100.0F;
		ownerPlayer.y = 200.0F;
		ownerPlayer.weaponType = common::game::WeaponType::Basic;

		const server::game::BulletState bulletState = simulation.CreateBullet(
			10,
			ownerPlayer.playerId,
			1,
			ownerPlayer,
			3.0F,
			4.0F,
			weaponRuleConfig
		);

		const common::game::WeaponRule& weaponRule = weaponRuleConfig.basicWeaponRule;

		tests::Expect(result, bulletState.bulletId == 10, "GameSimulation: CreateBullet bulletId");
		tests::Expect(result, bulletState.ownerPlayerId == 1, "GameSimulation: CreateBullet ownerPlayerId");
		tests::Expect(result, bulletState.roomId == 1, "GameSimulation: CreateBullet roomId");
		tests::Expect(result, bulletState.x == ownerPlayer.x, "GameSimulation: CreateBullet x");
		tests::Expect(result, bulletState.y == ownerPlayer.y, "GameSimulation: CreateBullet y");
		tests::Expect(result, IsNearlyEqual(bulletState.velocityX, weaponRule.bulletSpeed * 0.6F),
			"GameSimulation: CreateBullet velocityX normalized");
		tests::Expect(result, IsNearlyEqual(bulletState.velocityY, weaponRule.bulletSpeed * 0.8F),
			"GameSimulation: CreateBullet velocityY normalized");
		tests::Expect(result, bulletState.damage == weaponRule.bulletDamage, "GameSimulation: CreateBullet damage");
		tests::Expect(result, bulletState.radius == weaponRule.bulletRadius, "GameSimulation: CreateBullet radius");
	}

	void RunCreateBulletFallbackDirectionTest(tests::DebugTestResult& result)
	{
		server::game::GameSimulation simulation;
		server::config::WeaponRuleConfig weaponRuleConfig{};

		server::game::PlayerState ownerPlayer{};
		ownerPlayer.playerId = 1;
		ownerPlayer.weaponType = common::game::WeaponType::Basic;

		const server::game::BulletState bulletState = simulation.CreateBullet(
			11,
			ownerPlayer.playerId,
			1,
			ownerPlayer,
			0.0F,
			0.0F,
			weaponRuleConfig
		);

		const common::game::WeaponRule& weaponRule = weaponRuleConfig.basicWeaponRule;

		tests::Expect(result, IsNearlyEqual(bulletState.velocityX, weaponRule.bulletSpeed),
			"GameSimulation: CreateBullet fallback velocityX");
		tests::Expect(result, IsNearlyEqual(bulletState.velocityY, 0.0F), "GameSimulation: CreateBullet fallback velocityY");
	}

	void RunBulletLifetimeRemoveTest(tests::DebugTestResult& result)
	{
		server::game::GameSimulation simulation;
		server::game::GameWorld gameWorld;
		server::config::GameRuleConfig gameRuleConfig{};
		PeerTable peerTable;

		server::game::BulletState bulletState{};
		bulletState.bulletId = 1;
		bulletState.ownerPlayerId = 1;
		bulletState.roomId = 1;
		bulletState.x = 100.0F;
		bulletState.y = 100.0F;
		bulletState.remainingLifeSeconds = 0.05F;
		bulletState.radius = common::game::defaultBasicBulletRadius;

		gameWorld.AddBullet(bulletState);

		simulation.UpdateBullets(0.1F, peerTable, gameWorld, gameRuleConfig);

		tests::Expect(result, gameWorld.GetBulletCount() == 0, "GameSimulation: expired bullet removed");
	}

	void RunBulletHitPlayerTest(tests::DebugTestResult& result)
	{
		server::game::GameSimulation simulation;
		server::game::GameWorld gameWorld;
		server::config::GameRuleConfig gameRuleConfig{};
		PeerTable peerTable;

		constexpr common::game::PlayerId ownerPlayerId = 1;
		constexpr common::game::PlayerId targetPlayerId = 2;
		constexpr common::game::RoomId roomId = 1;

		server::game::PlayerState ownerPlayer = MakePlayer(ownerPlayerId, 100.0F, 100.0F);
		server::game::PlayerState targetPlayer = MakePlayer(targetPlayerId, 130.0F, 100.0F);
		targetPlayer.hp = gameRuleConfig.initialPlayerHp;

		gameWorld.UpsertPlayer(ownerPlayer);
		gameWorld.UpsertPlayer(targetPlayer);

		AddJoinedPeer(peerTable, ownerPlayerId, roomId);
		AddJoinedPeer(peerTable, targetPlayerId, roomId);

		server::game::BulletState bulletState{};
		bulletState.bulletId = 1;
		bulletState.ownerPlayerId = ownerPlayerId;
		bulletState.roomId = roomId;
		bulletState.x = targetPlayer.x;
		bulletState.y = targetPlayer.y;
		bulletState.velocityX = 0.0F;
		bulletState.velocityY = 0.0F;
		bulletState.remainingLifeSeconds = 1.0F;
		bulletState.damage = 1;
		bulletState.radius = common::game::defaultBasicBulletRadius;

		gameWorld.AddBullet(bulletState);

		simulation.UpdateBullets(0.0F, peerTable, gameWorld, gameRuleConfig);

		const server::game::PlayerState* updatedTargetPlayer = gameWorld.FindPlayer(targetPlayerId);

		tests::Expect(result, updatedTargetPlayer != nullptr, "GameSimulation: hit target exists");
		tests::Expect(result, gameWorld.GetBulletCount() == 0, "GameSimulation: hit bullet removed");
		tests::Expect(result, gameWorld.GetPendingImpactEffectCount() == 1, "GameSimulation: hit impact effect spawned");

		if (updatedTargetPlayer == nullptr)
		{
			return;
		}

		tests::Expect(result, updatedTargetPlayer->hp == gameRuleConfig.initialPlayerHp - 1, "GameSimulation: hit hp decreased");
		tests::Expect(result, updatedTargetPlayer->hitFlashRemainingSeconds > 0.0F, "GameSimulation: hit flash timer set");
		tests::Expect(result, !updatedTargetPlayer->isDead, "GameSimulation: hit target still alive");
	}

	void RunBulletKillPlayerTest(tests::DebugTestResult& result)
	{
		server::game::GameSimulation simulation;
		server::game::GameWorld gameWorld;
		server::config::GameRuleConfig gameRuleConfig{};
		PeerTable peerTable;

		constexpr common::game::PlayerId ownerPlayerId = 1;
		constexpr common::game::PlayerId targetPlayerId = 2;
		constexpr common::game::RoomId roomId = 1;

		server::game::PlayerState ownerPlayer = MakePlayer(ownerPlayerId, 100.0F, 100.0F);
		server::game::PlayerState targetPlayer = MakePlayer(targetPlayerId, 130.0F, 100.0F);
		targetPlayer.hp = 1;

		gameWorld.UpsertPlayer(ownerPlayer);
		gameWorld.UpsertPlayer(targetPlayer);

		AddJoinedPeer(peerTable, ownerPlayerId, roomId);
		AddJoinedPeer(peerTable, targetPlayerId, roomId);

		server::game::BulletState bulletState{};
		bulletState.bulletId = 1;
		bulletState.ownerPlayerId = ownerPlayerId;
		bulletState.roomId = roomId;
		bulletState.x = targetPlayer.x;
		bulletState.y = targetPlayer.y;
		bulletState.velocityX = 0.0F;
		bulletState.velocityY = 0.0F;
		bulletState.remainingLifeSeconds = 1.0F;
		bulletState.damage = 1;
		bulletState.radius = common::game::defaultBasicBulletRadius;

		gameWorld.AddBullet(bulletState);

		simulation.UpdateBullets(0.0F, peerTable, gameWorld, gameRuleConfig);

		const server::game::PlayerState* updatedOwnerPlayer = gameWorld.FindPlayer(ownerPlayerId);
		const server::game::PlayerState* updatedTargetPlayer = gameWorld.FindPlayer(targetPlayerId);

		tests::Expect(result, updatedOwnerPlayer != nullptr, "GameSimulation: kill owner exists");
		tests::Expect(result, updatedTargetPlayer != nullptr, "GameSimulation: kill target exists");

		if (updatedOwnerPlayer == nullptr || updatedTargetPlayer == nullptr)
		{
			return;
		}

		tests::Expect(result, updatedTargetPlayer->hp == 0, "GameSimulation: kill target hp zero");
		tests::Expect(result, updatedTargetPlayer->isDead, "GameSimulation: kill target dead");
		tests::Expect(result, updatedTargetPlayer->deathCount == 1, "GameSimulation: kill target deathCount");
		tests::Expect(result, updatedOwnerPlayer->killCount == 1, "GameSimulation: kill owner killCount");
		tests::Expect(result, updatedTargetPlayer->inputFlags == common::game::InputFlags::None, "GameSimulation: kill clears input");
		tests::Expect(result, updatedTargetPlayer->respawnRemainingSeconds > 0.0F, "GameSimulation: kill sets respawn timer");
	}

	void RunInvinciblePlayerIgnoresBulletTest(tests::DebugTestResult& result)
	{
		server::game::GameSimulation simulation;
		server::game::GameWorld gameWorld;
		server::config::GameRuleConfig gameRuleConfig{};
		PeerTable peerTable;

		constexpr common::game::PlayerId ownerPlayerId = 1;
		constexpr common::game::PlayerId targetPlayerId = 2;
		constexpr common::game::RoomId roomId = 1;

		server::game::PlayerState ownerPlayer = MakePlayer(ownerPlayerId, 100.0F, 100.0F);
		server::game::PlayerState targetPlayer = MakePlayer(targetPlayerId, 130.0F, 100.0F);
		targetPlayer.invincibilityRemainingSeconds = 1.0F;

		gameWorld.UpsertPlayer(ownerPlayer);
		gameWorld.UpsertPlayer(targetPlayer);

		AddJoinedPeer(peerTable, ownerPlayerId, roomId);
		AddJoinedPeer(peerTable, targetPlayerId, roomId);

		server::game::BulletState bulletState{};
		bulletState.bulletId = 1;
		bulletState.ownerPlayerId = ownerPlayerId;
		bulletState.roomId = roomId;
		bulletState.x = targetPlayer.x;
		bulletState.y = targetPlayer.y;
		bulletState.velocityX = 0.0F;
		bulletState.velocityY = 0.0F;
		bulletState.remainingLifeSeconds = 1.0F;
		bulletState.damage = 1;
		bulletState.radius = common::game::defaultBasicBulletRadius;

		gameWorld.AddBullet(bulletState);

		simulation.UpdateBullets(0.0F, peerTable, gameWorld, gameRuleConfig);

		const server::game::PlayerState* updatedTargetPlayer = gameWorld.FindPlayer(targetPlayerId);

		tests::Expect(result, updatedTargetPlayer != nullptr, "GameSimulation: invincible target exists");
		tests::Expect(result, gameWorld.GetBulletCount() == 1, "GameSimulation: invincible bullet remains");
		tests::Expect(result, gameWorld.GetPendingImpactEffectCount() == 0, "GameSimulation: invincible no impact");

		if (updatedTargetPlayer == nullptr)
		{
			return;
		}

		tests::Expect(result, updatedTargetPlayer->hp == gameRuleConfig.initialPlayerHp, "GameSimulation: invincible hp unchanged");
		tests::Expect(result, !updatedTargetPlayer->isDead, "GameSimulation: invincible not dead");
	}

	void RunRespawnTest(tests::DebugTestResult& result)
	{
		server::game::GameSimulation simulation;
		server::game::GameWorld gameWorld;
		server::config::GameRuleConfig gameRuleConfig{};
		PeerTable peerTable;

		constexpr common::game::PlayerId playerId = 1;
		constexpr common::game::RoomId roomId = 1;

		server::game::PlayerState playerState = MakePlayer(playerId, 0.0F, 0.0F);
		playerState.hp = 0;
		playerState.isDead = true;
		playerState.respawnRemainingSeconds = 0.05F;
		playerState.inputFlags = common::game::InputFlags::Up;
		playerState.lastMoveDirectionX = 0.0F;
		playerState.lastMoveDirectionY = -1.0F;

		gameWorld.UpsertPlayer(playerState);
		AddJoinedPeer(peerTable, playerId, roomId);

		simulation.UpdateRespawns(0.1F, peerTable, gameWorld, gameRuleConfig);

		const server::game::PlayerState* updatedPlayerState = gameWorld.FindPlayer(playerId);

		tests::Expect(result, updatedPlayerState != nullptr, "GameSimulation: respawn player exists");
		tests::Expect(result, gameWorld.GetPendingImpactEffectCount() == 1, "GameSimulation: respawn spawn effect");

		if (updatedPlayerState == nullptr)
		{
			return;
		}

		tests::Expect(result, !updatedPlayerState->isDead, "GameSimulation: respawn not dead");
		tests::Expect(result, updatedPlayerState->hp == gameRuleConfig.initialPlayerHp, "GameSimulation: respawn hp");
		tests::Expect(result, updatedPlayerState->respawnRemainingSeconds == 0.0F, "GameSimulation: respawn timer cleared");
		tests::Expect(result, updatedPlayerState->invincibilityRemainingSeconds > 0.0F, "GameSimulation: respawn invincible");
		tests::Expect(result, updatedPlayerState->inputFlags == common::game::InputFlags::None, "GameSimulation: respawn input cleared");
		tests::Expect(result, IsNearlyEqual(updatedPlayerState->lastMoveDirectionX, 1.0F), "GameSimulation: respawn direction x");
		tests::Expect(result, IsNearlyEqual(updatedPlayerState->lastMoveDirectionY, 0.0F), "GameSimulation: respawn direction y");
	}

	void RunTimerClampTest(tests::DebugTestResult& result)
	{
		server::game::GameSimulation simulation;
		server::game::GameWorld gameWorld;

		server::game::PlayerState playerState{};
		playerState.playerId = 1;
		playerState.invincibilityRemainingSeconds = 0.05F;
		playerState.hitFlashRemainingSeconds = 0.05F;
		playerState.fireCooldownRemainingSeconds = 0.05F;

		gameWorld.UpsertPlayer(playerState);

		simulation.UpdatePlayerTimers(0.1F, gameWorld);

		const server::game::PlayerState* updatedPlayerState = gameWorld.FindPlayer(1);

		tests::Expect(result, updatedPlayerState != nullptr, "GameSimulation: timer player exists");

		if (updatedPlayerState == nullptr)
		{
			return;
		}

		tests::Expect(result, updatedPlayerState->invincibilityRemainingSeconds == 0.0F, "GameSimulation: invincibility timer clamp");
		tests::Expect(result, updatedPlayerState->hitFlashRemainingSeconds == 0.0F, "GameSimulation: hit flash timer clamp");
		tests::Expect(result, updatedPlayerState->fireCooldownRemainingSeconds == 0.0F, "GameSimulation: fire cooldown timer clamp");
	}
}

namespace tests::server
{
	tests::DebugTestResult RunGameSimulationTests()
	{
		tests::DebugTestResult result{};

		RunCreateBulletDirectionTest(result);
		RunCreateBulletFallbackDirectionTest(result);
		RunBulletLifetimeRemoveTest(result);
		RunBulletHitPlayerTest(result);
		RunBulletKillPlayerTest(result);
		RunInvinciblePlayerIgnoresBulletTest(result);
		RunRespawnTest(result);
		RunTimerClampTest(result);

		return result;
	}
}