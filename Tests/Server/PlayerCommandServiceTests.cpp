#include "PlayerCommandServiceTests.h"

#include <WinSock2.h>

#include <chrono>
#include <cmath>
#include <cstdint>

#include <Common/Diagnostics/DebugTestResult.h>
#include <Common/Game/InputFlags.h>
#include <Common/Game/WeaponRules.h>
#include <Common/Net/Endpoint.h>
#include <Common/Packet/GamePacket.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/PlayerState.h>
#include <Server/Net/PeerRoomManager.h>
#include <Server/Net/PeerState.h>
#include <Server/Net/PlayerCommandService.h>

namespace
{
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	[[nodiscard]] bool IsNearlyEqual(float lhs, float rhs, float epsilon = 0.001F) noexcept
	{
		return std::fabs(lhs - rhs) <= epsilon;
	}

	[[nodiscard]] common::net::EndpointKey MakeEndpointKey(std::uint32_t index) noexcept
	{
		common::net::EndpointKey endpointKey{};
		endpointKey.address = 0x7F000001 + index;
		endpointKey.port = static_cast<std::uint16_t>(10000 + index);
		return endpointKey;
	}

	[[nodiscard]] sockaddr_in MakeRemoteAddress(const common::net::EndpointKey& endpointKey) noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_addr.S_un.S_addr = endpointKey.address;
		remoteAddress.sin_port = endpointKey.port;
		return remoteAddress;
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

	void AddJoinedPeerAndPlayer(
		server::net::PeerRoomManager& peerRoomManager,
		server::game::GameWorld& gameWorld,
		const common::net::EndpointKey& endpointKey,
		common::game::PlayerId playerId,
		common::game::RoomId roomId,
		TimePoint currentTime
	)
	{
		const sockaddr_in remoteAddress = MakeRemoteAddress(endpointKey);

		peerRoomManager.UpsertJoinedPeer(
			remoteAddress,
			endpointKey,
			playerId,
			roomId,
			currentTime
		);

		gameWorld.UpsertPlayer(MakePlayer(playerId, 100.0F, 100.0F));
	}

	void RunApplyInputCommandSuccessTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PlayerCommandService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(1);
		constexpr common::game::PlayerId playerId = 1;
		constexpr common::game::RoomId roomId = 1;
		const TimePoint joinTime = Clock::now();
		const TimePoint commandTime = joinTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(peerRoomManager, gameWorld, endpointKey, playerId, roomId, joinTime);

		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 1;
		packet.inputFlags = common::game::InputFlags::Right | common::game::InputFlags::Up;

		const bool applied = service.ApplyInputCommand(
			endpointKey,
			packet,
			peerRoomManager,
			gameWorld,
			commandTime
		);

		common::diagnostics::Expect(result, applied, "PlayerCommandService: input command applied");

		const server::net::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);
		const server::game::PlayerState* playerState = gameWorld.FindPlayer(playerId);

		common::diagnostics::Expect(result, peerState != nullptr, "PlayerCommandService: input peer exists");
		common::diagnostics::Expect(result, playerState != nullptr, "PlayerCommandService: input player exists");

		if (peerState != nullptr)
		{
			common::diagnostics::Expect(result, peerState->lastInputSequence == 1, "PlayerCommandService: input sequence updated");
			common::diagnostics::Expect(result, peerState->lastRecvTime == commandTime, "PlayerCommandService: input recv time updated");
		}

		if (playerState != nullptr)
		{
			common::diagnostics::Expect(result, playerState->inputFlags == packet.inputFlags, "PlayerCommandService: input flags updated");
			common::diagnostics::Expect(result, IsNearlyEqual(playerState->lastMoveDirectionX, 0.707106F),
				"PlayerCommandService: input direction x updated");
			common::diagnostics::Expect(result, IsNearlyEqual(playerState->lastMoveDirectionY, -0.707106F),
				"PlayerCommandService: input direction y updated");
		}
	}

	void RunApplyInputCommandStaleSequenceRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PlayerCommandService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(2);
		constexpr common::game::PlayerId playerId = 2;
		constexpr common::game::RoomId roomId = 1;
		const TimePoint joinTime = Clock::now();
		const TimePoint firstCommandTime = joinTime + std::chrono::seconds(1);
		const TimePoint staleCommandTime = firstCommandTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(peerRoomManager, gameWorld, endpointKey, playerId, roomId, joinTime);

		common::packet::InputCommandPacket firstPacket{};
		firstPacket.inputSequence = 10;
		firstPacket.inputFlags = common::game::InputFlags::Right;

		common::packet::InputCommandPacket stalePacket{};
		stalePacket.inputSequence = 10;
		stalePacket.inputFlags = common::game::InputFlags::Left;

		const bool firstApplied = service.ApplyInputCommand(
			endpointKey,
			firstPacket,
			peerRoomManager,
			gameWorld,
			firstCommandTime
		);

		const bool staleApplied = service.ApplyInputCommand(
			endpointKey,
			stalePacket,
			peerRoomManager,
			gameWorld,
			staleCommandTime
		);

		const server::net::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);
		const server::game::PlayerState* playerState = gameWorld.FindPlayer(playerId);

		common::diagnostics::Expect(result, firstApplied, "PlayerCommandService: stale base input applied");
		common::diagnostics::Expect(result, !staleApplied, "PlayerCommandService: stale input rejected");

		if (peerState != nullptr)
		{
			common::diagnostics::Expect(result, peerState->lastInputSequence == 10, "PlayerCommandService: stale sequence unchanged");
			common::diagnostics::Expect(result, peerState->lastRecvTime == firstCommandTime, "PlayerCommandService: stale recv time unchanged");
		}

		if (playerState != nullptr)
		{
			common::diagnostics::Expect(result, playerState->inputFlags == common::game::InputFlags::Right,
				"PlayerCommandService: stale input flags unchanged");
		}
	}

	void RunApplyInputCommandDeadPlayerRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PlayerCommandService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(3);
		constexpr common::game::PlayerId playerId = 3;
		constexpr common::game::RoomId roomId = 1;
		const TimePoint joinTime = Clock::now();
		const TimePoint commandTime = joinTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(peerRoomManager, gameWorld, endpointKey, playerId, roomId, joinTime);

		server::game::PlayerState* playerState = gameWorld.FindPlayer(playerId);
		if (playerState != nullptr)
		{
			playerState->isDead = true;
		}

		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 1;
		packet.inputFlags = common::game::InputFlags::Down;

		const bool applied = service.ApplyInputCommand(
			endpointKey,
			packet,
			peerRoomManager,
			gameWorld,
			commandTime
		);

		const server::net::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);

		common::diagnostics::Expect(result, !applied, "PlayerCommandService: dead player input rejected");

		if (peerState != nullptr)
		{
			common::diagnostics::Expect(result, peerState->lastInputSequence == 0, "PlayerCommandService: dead input sequence unchanged");
			common::diagnostics::Expect(result, peerState->lastRecvTime == joinTime, "PlayerCommandService: dead input recv time unchanged");
		}
	}

	void RunApplyInputCommandUnknownPeerRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PlayerCommandService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(4);
		const TimePoint commandTime = Clock::now();

		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 1;
		packet.inputFlags = common::game::InputFlags::Right;

		const bool applied = service.ApplyInputCommand(
			endpointKey,
			packet,
			peerRoomManager,
			gameWorld,
			commandTime
		);

		common::diagnostics::Expect(result, !applied, "PlayerCommandService: unknown peer input rejected");
		common::diagnostics::Expect(result, peerRoomManager.GetPeerCount() == 0, "PlayerCommandService: unknown input peer count");
		common::diagnostics::Expect(result, gameWorld.GetPlayerCount() == 0, "PlayerCommandService: unknown input player count");
	}

	void RunFireBulletSuccessTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PlayerCommandService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(5);
		constexpr common::game::PlayerId playerId = 5;
		constexpr common::game::RoomId roomId = 2;
		const TimePoint joinTime = Clock::now();
		const TimePoint fireTime = joinTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(peerRoomManager, gameWorld, endpointKey, playerId, roomId, joinTime);

		server::game::PlayerState* playerState = gameWorld.FindPlayer(playerId);
		if (playerState != nullptr)
		{
			playerState->x = 50.0F;
			playerState->y = 60.0F;
			playerState->lastMoveDirectionX = 0.0F;
			playerState->lastMoveDirectionY = 1.0F;
		}

		const bool fired = service.FireBullet(
			endpointKey,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			weaponRuleConfig,
			fireTime
		);

		const server::net::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);
		const server::game::PlayerState* updatedPlayerState = gameWorld.FindPlayer(playerId);
		const server::game::GameWorld::BulletStateList& bulletStateList = gameWorld.GetBulletStateList();

		common::diagnostics::Expect(result, fired, "PlayerCommandService: fire succeeds");
		common::diagnostics::Expect(result, gameWorld.GetBulletCount() == 1, "PlayerCommandService: fire creates bullet");

		if (peerState != nullptr)
		{
			common::diagnostics::Expect(result, peerState->lastRecvTime == fireTime, "PlayerCommandService: fire recv time updated");
		}

		if (updatedPlayerState != nullptr)
		{
			common::diagnostics::Expect(result, updatedPlayerState->fireCooldownRemainingSeconds == weaponRuleConfig.basicWeaponRule.fireCooldownSeconds,
				"PlayerCommandService: fire cooldown set");
		}

		if (!bulletStateList.empty())
		{
			const server::game::BulletState& bulletState = bulletStateList.front();

			common::diagnostics::Expect(result, bulletState.bulletId == 1, "PlayerCommandService: fire bullet id");
			common::diagnostics::Expect(result, bulletState.ownerPlayerId == playerId, "PlayerCommandService: fire owner id");
			common::diagnostics::Expect(result, bulletState.roomId == roomId, "PlayerCommandService: fire room id");
			common::diagnostics::Expect(result, IsNearlyEqual(bulletState.x, 50.0F), "PlayerCommandService: fire bullet x");
			common::diagnostics::Expect(result, IsNearlyEqual(bulletState.y, 60.0F), "PlayerCommandService: fire bullet y");
			common::diagnostics::Expect(result, IsNearlyEqual(bulletState.velocityX, 0.0F), "PlayerCommandService: fire velocity x");
			common::diagnostics::Expect(result, IsNearlyEqual(bulletState.velocityY, weaponRuleConfig.basicWeaponRule.bulletSpeed),
				"PlayerCommandService: fire velocity y");
			common::diagnostics::Expect(result, bulletState.damage == weaponRuleConfig.basicWeaponRule.bulletDamage,
				"PlayerCommandService: fire damage");
			common::diagnostics::Expect(result, bulletState.radius == weaponRuleConfig.basicWeaponRule.bulletRadius,
				"PlayerCommandService: fire radius");
		}
	}

	void RunFireBulletCooldownRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PlayerCommandService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(6);
		constexpr common::game::PlayerId playerId = 6;
		constexpr common::game::RoomId roomId = 1;
		const TimePoint joinTime = Clock::now();
		const TimePoint fireTime = joinTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(peerRoomManager, gameWorld, endpointKey, playerId, roomId, joinTime);

		server::game::PlayerState* playerState = gameWorld.FindPlayer(playerId);
		if (playerState != nullptr)
		{
			playerState->fireCooldownRemainingSeconds = 1.0F;
		}

		const bool fired = service.FireBullet(
			endpointKey,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			weaponRuleConfig,
			fireTime
		);

		const server::net::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);

		common::diagnostics::Expect(result, !fired, "PlayerCommandService: cooldown fire rejected");
		common::diagnostics::Expect(result, gameWorld.GetBulletCount() == 0, "PlayerCommandService: cooldown creates no bullet");

		if (peerState != nullptr)
		{
			common::diagnostics::Expect(result, peerState->lastRecvTime == fireTime, "PlayerCommandService: cooldown still refreshes recv time");
		}
	}

	void RunFireBulletDeadPlayerRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PlayerCommandService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(7);
		constexpr common::game::PlayerId playerId = 7;
		constexpr common::game::RoomId roomId = 1;
		const TimePoint joinTime = Clock::now();
		const TimePoint fireTime = joinTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(peerRoomManager, gameWorld, endpointKey, playerId, roomId, joinTime);

		server::game::PlayerState* playerState = gameWorld.FindPlayer(playerId);
		if (playerState != nullptr)
		{
			playerState->isDead = true;
		}

		const bool fired = service.FireBullet(
			endpointKey,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			weaponRuleConfig,
			fireTime
		);

		const server::net::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);

		common::diagnostics::Expect(result, !fired, "PlayerCommandService: dead player fire rejected");
		common::diagnostics::Expect(result, gameWorld.GetBulletCount() == 0, "PlayerCommandService: dead player creates no bullet");

		if (peerState != nullptr)
		{
			common::diagnostics::Expect(result, peerState->lastRecvTime == joinTime, "PlayerCommandService: dead fire recv time unchanged");
		}
	}

	void RunFireBulletUnknownPeerRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PlayerCommandService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(8);
		const TimePoint fireTime = Clock::now();

		const bool fired = service.FireBullet(
			endpointKey,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			weaponRuleConfig,
			fireTime
		);

		common::diagnostics::Expect(result, !fired, "PlayerCommandService: unknown peer fire rejected");
		common::diagnostics::Expect(result, peerRoomManager.GetPeerCount() == 0, "PlayerCommandService: unknown fire peer count");
		common::diagnostics::Expect(result, gameWorld.GetBulletCount() == 0, "PlayerCommandService: unknown fire bullet count");
	}

	void RunFireBulletMissingPlayerRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PlayerCommandService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(9);
		constexpr common::game::PlayerId playerId = 9;
		constexpr common::game::RoomId roomId = 1;
		const TimePoint joinTime = Clock::now();
		const TimePoint fireTime = joinTime + std::chrono::seconds(1);
		const sockaddr_in remoteAddress = MakeRemoteAddress(endpointKey);

		peerRoomManager.UpsertJoinedPeer(
			remoteAddress,
			endpointKey,
			playerId,
			roomId,
			joinTime
		);

		const bool fired = service.FireBullet(
			endpointKey,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			weaponRuleConfig,
			fireTime
		);

		const server::net::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);

		common::diagnostics::Expect(result, !fired, "PlayerCommandService: missing player fire rejected");
		common::diagnostics::Expect(result, gameWorld.GetBulletCount() == 0, "PlayerCommandService: missing player bullet count");

		if (peerState != nullptr)
		{
			common::diagnostics::Expect(result, peerState->lastRecvTime == joinTime, "PlayerCommandService: missing player recv time unchanged");
		}
	}
}

namespace tests::server
{
	common::diagnostics::DebugTestResult RunPlayerCommandServiceTests()
	{
		common::diagnostics::DebugTestResult result{};

		RunApplyInputCommandSuccessTest(result);
		RunApplyInputCommandStaleSequenceRejectedTest(result);
		RunApplyInputCommandDeadPlayerRejectedTest(result);
		RunApplyInputCommandUnknownPeerRejectedTest(result);
		RunFireBulletSuccessTest(result);
		RunFireBulletCooldownRejectedTest(result);
		RunFireBulletDeadPlayerRejectedTest(result);
		RunFireBulletUnknownPeerRejectedTest(result);
		RunFireBulletMissingPlayerRejectedTest(result);

		return result;
	}
}