#include "PlayerCommandServiceTests.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>

#include <Common/Game/InputFlags.h>
#include <Common/Game/WeaponRules.h>
#include <Common/Net/EndpointKey.h>

#include <Server/Game/GameWorld.h>
#include <Server/Game/PlayerState.h>
#include <Server/Service/PeerRoomManager.h>
#include <Server/Service/PeerState.h>
#include <Server/Service/PlayerCommandService.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	using PeerRoomManager = server::service::PeerRoomManager;
	using PeerState = server::service::PeerState;
	using PlayerCommandService = server::service::PlayerCommandService;

	[[nodiscard]] bool IsNearlyEqual(float lhs, float rhs, float epsilon = 0.001F) noexcept
	{
		return std::fabs(lhs - rhs) <= epsilon;
	}

	[[nodiscard]] constexpr common::net::EndpointKey MakeEndpointKey(std::uint32_t index) noexcept
	{
		return common::net::EndpointKey{
			.address = 0x7F000001 + index,
			.port = static_cast<std::uint16_t>(10000 + index),
		};
	}

	[[nodiscard]] server::game::PlayerState MakePlayer(
		common::game::PlayerId playerId,
		float x,
		float y
	)
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
		PeerRoomManager& peerRoomManager,
		server::game::GameWorld& gameWorld,
		const common::net::EndpointKey& endpointKey,
		common::game::PlayerId playerId,
		common::game::RoomId roomId,
		TimePoint currentTime
	)
	{
		PeerState& peerState = peerRoomManager.UpsertJoinedPeer(
			endpointKey,
			playerId,
			roomId,
			currentTime
		);

		peerState.persistentPlayerId =
			static_cast<std::int64_t>(5000 + playerId);

		gameWorld.UpsertPlayer(
			MakePlayer(
				playerId,
				100.0F,
				100.0F
			)
		);
	}

	void RunApplyInputCommandSuccessTest(tests::DebugTestResult& result)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(1);
		const TimePoint joinTime = Clock::now();
		const TimePoint commandTime = joinTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(
			peerRoomManager,
			gameWorld,
			endpointKey,
			1,
			1,
			joinTime
		);

		const common::game::InputFlags inputFlags =
			common::game::InputFlags::Right |
			common::game::InputFlags::Up;

		const bool applied = service.ApplyInputCommand(
			endpointKey,
			1,
			inputFlags,
			peerRoomManager,
			gameWorld,
			commandTime
		);

		tests::Expect(
			result,
			applied,
			"PlayerCommandService: input command applied"
		);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		const server::game::PlayerState* playerState =
			gameWorld.FindPlayer(1);

		tests::Expect(
			result,
			peerState != nullptr,
			"PlayerCommandService: input peer exists"
		);

		tests::Expect(
			result,
			playerState != nullptr,
			"PlayerCommandService: input player exists"
		);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastAcceptedInputSequence == 1,
				"PlayerCommandService: accepted input sequence updated"
			);

			tests::Expect(
				result,
				peerState->lastProcessedInputSequence == 0,
				"PlayerCommandService: processed input sequence unchanged"
			);

			tests::Expect(
				result,
				peerState->lastRecvTime == commandTime,
				"PlayerCommandService: input recv time updated"
			);
		}

		if (playerState != nullptr)
		{
			tests::Expect(
				result,
				playerState->inputFlags == inputFlags,
				"PlayerCommandService: input flags updated"
			);

			tests::Expect(
				result,
				IsNearlyEqual(
					playerState->lastMoveDirectionX,
					0.707106F
				),
				"PlayerCommandService: input direction x updated"
			);

			tests::Expect(
				result,
				IsNearlyEqual(
					playerState->lastMoveDirectionY,
					-0.707106F
				),
				"PlayerCommandService: input direction y updated"
			);
		}
	}

	void RunApplyInputCommandStaleSequenceRejectedTest(tests::DebugTestResult& result)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(2);
		const TimePoint joinTime = Clock::now();
		const TimePoint firstCommandTime =
			joinTime + std::chrono::seconds(1);

		const TimePoint staleCommandTime =
			firstCommandTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(
			peerRoomManager,
			gameWorld,
			endpointKey,
			2,
			1,
			joinTime
		);

		const bool firstApplied = service.ApplyInputCommand(
			endpointKey,
			10,
			common::game::InputFlags::Right,
			peerRoomManager,
			gameWorld,
			firstCommandTime
		);

		const bool staleApplied = service.ApplyInputCommand(
			endpointKey,
			10,
			common::game::InputFlags::Left,
			peerRoomManager,
			gameWorld,
			staleCommandTime
		);

		tests::Expect(
			result,
			firstApplied,
			"PlayerCommandService: stale base input applied"
		);

		tests::Expect(
			result,
			!staleApplied,
			"PlayerCommandService: stale input rejected"
		);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		const server::game::PlayerState* playerState =
			gameWorld.FindPlayer(2);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastAcceptedInputSequence == 10,
				"PlayerCommandService: stale accepted sequence unchanged"
			);

			tests::Expect(
				result,
				peerState->lastProcessedInputSequence == 0,
				"PlayerCommandService: stale processed sequence unchanged"
			);

			tests::Expect(
				result,
				peerState->lastRecvTime == firstCommandTime,
				"PlayerCommandService: stale recv time unchanged"
			);
		}

		if (playerState != nullptr)
		{
			tests::Expect(
				result,
				playerState->inputFlags ==
				common::game::InputFlags::Right,
				"PlayerCommandService: stale input flags unchanged"
			);
		}
	}

	void RunApplyInputCommandWrappedSequenceAcceptedTest(tests::DebugTestResult& result)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(10);
		const TimePoint joinTime = Clock::now();
		const TimePoint wrappedCommandTime =
			joinTime + std::chrono::seconds(1);

		const TimePoint staleCommandTime =
			wrappedCommandTime + std::chrono::seconds(1);

		constexpr common::game::PlayerId playerId = 10;

		AddJoinedPeerAndPlayer(
			peerRoomManager,
			gameWorld,
			endpointKey,
			playerId,
			1,
			joinTime
		);

		PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr,
			"PlayerCommandService: wrap peer exists"
		);

		if (peerState == nullptr)
		{
			return;
		}

		constexpr std::uint32_t maxSequence =
			std::numeric_limits<std::uint32_t>::max();

		peerState->lastAcceptedInputSequence = maxSequence;
		peerState->lastProcessedInputSequence = maxSequence - 1;

		const bool wrappedApplied = service.ApplyInputCommand(
			endpointKey,
			0,
			common::game::InputFlags::Right,
			peerRoomManager,
			gameWorld,
			wrappedCommandTime
		);

		const bool previousSequenceApplied = service.ApplyInputCommand(
			endpointKey,
			maxSequence,
			common::game::InputFlags::Left,
			peerRoomManager,
			gameWorld,
			staleCommandTime
		);

		tests::Expect(
			result,
			wrappedApplied,
			"PlayerCommandService: wrapped zero sequence accepted"
		);

		tests::Expect(
			result,
			!previousSequenceApplied,
			"PlayerCommandService: pre-wrap sequence rejected after wrap"
		);

		tests::Expect(
			result,
			peerState->lastAcceptedInputSequence == 0,
			"PlayerCommandService: wrapped accepted sequence becomes zero"
		);

		tests::Expect(
			result,
			peerState->lastProcessedInputSequence == maxSequence - 1,
			"PlayerCommandService: wrapped input does not advance processed sequence"
		);

		tests::Expect(
			result,
			peerState->lastRecvTime == wrappedCommandTime,
			"PlayerCommandService: rejected pre-wrap input does not refresh recv time"
		);

		const server::game::PlayerState* playerState =
			gameWorld.FindPlayer(playerId);

		if (playerState != nullptr)
		{
			tests::Expect(
				result,
				playerState->inputFlags ==
				common::game::InputFlags::Right,
				"PlayerCommandService: rejected pre-wrap input does not replace input flags"
			);
		}
	}

	void RunApplyInputCommandDeadPlayerRejectedTest(tests::DebugTestResult& result)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(3);
		const TimePoint joinTime = Clock::now();
		const TimePoint commandTime =
			joinTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(
			peerRoomManager,
			gameWorld,
			endpointKey,
			3,
			1,
			joinTime
		);

		server::game::PlayerState* playerState =
			gameWorld.FindPlayer(3);

		if (playerState != nullptr)
		{
			playerState->isDead = true;
		}

		const bool applied = service.ApplyInputCommand(
			endpointKey,
			1,
			common::game::InputFlags::Down,
			peerRoomManager,
			gameWorld,
			commandTime
		);

		tests::Expect(
			result,
			!applied,
			"PlayerCommandService: dead player input rejected"
		);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastAcceptedInputSequence == 0,
				"PlayerCommandService: dead accepted input sequence unchanged"
			);

			tests::Expect(
				result,
				peerState->lastProcessedInputSequence == 0,
				"PlayerCommandService: dead processed input sequence unchanged"
			);

			tests::Expect(
				result,
				peerState->lastRecvTime == joinTime,
				"PlayerCommandService: dead input recv time unchanged"
			);
		}
	}

	void RunApplyInputCommandUnknownPeerRejectedTest(tests::DebugTestResult& result)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const bool applied = service.ApplyInputCommand(
			MakeEndpointKey(4),
			1,
			common::game::InputFlags::Right,
			peerRoomManager,
			gameWorld,
			Clock::now()
		);

		tests::Expect(
			result,
			!applied,
			"PlayerCommandService: unknown peer input rejected"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 0,
			"PlayerCommandService: unknown input peer count"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 0,
			"PlayerCommandService: unknown input player count"
		);
	}

	void RunFireBulletSuccessTest(tests::DebugTestResult& result)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(5);
		const TimePoint joinTime = Clock::now();
		const TimePoint fireTime =
			joinTime + std::chrono::seconds(1);

		constexpr common::game::PlayerId playerId = 5;
		constexpr common::game::RoomId roomId = 2;
		constexpr std::int64_t persistentPlayerId = 5000 + playerId;

		AddJoinedPeerAndPlayer(
			peerRoomManager,
			gameWorld,
			endpointKey,
			playerId,
			roomId,
			joinTime
		);

		server::game::PlayerState* playerState =
			gameWorld.FindPlayer(playerId);

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
			weaponRuleConfig,
			fireTime
		);

		tests::Expect(
			result,
			fired,
			"PlayerCommandService: fire succeeds"
		);

		tests::Expect(
			result,
			gameWorld.GetBulletCount() == 1,
			"PlayerCommandService: fire creates bullet"
		);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastRecvTime == fireTime,
				"PlayerCommandService: fire recv time updated"
			);
		}

		const server::game::PlayerState* updatedPlayerState =
			gameWorld.FindPlayer(playerId);

		if (updatedPlayerState != nullptr)
		{
			tests::Expect(
				result,
				updatedPlayerState->fireCooldownRemainingSeconds ==
				weaponRuleConfig.basicWeaponRule.fireCooldownSeconds,
				"PlayerCommandService: fire cooldown set"
			);
		}

		const server::game::GameWorld::BulletStateList& bulletStateList =
			gameWorld.GetBulletStateList();

		if (!bulletStateList.empty())
		{
			const server::game::BulletState& bulletState =
				bulletStateList.front();

			tests::Expect(
				result,
				bulletState.bulletId == 1,
				"PlayerCommandService: fire bullet id"
			);

			tests::Expect(
				result,
				bulletState.ownerPlayerId == playerId,
				"PlayerCommandService: fire owner id"
			);

			tests::Expect(
				result,
				bulletState.ownerPersistentPlayerId == persistentPlayerId,
				"PlayerCommandService: fire persistent owner id"
			);

			tests::Expect(
				result,
				bulletState.roomId == roomId,
				"PlayerCommandService: fire room id"
			);

			tests::Expect(
				result,
				IsNearlyEqual(bulletState.x, 50.0F),
				"PlayerCommandService: fire bullet x"
			);

			tests::Expect(
				result,
				IsNearlyEqual(bulletState.y, 60.0F),
				"PlayerCommandService: fire bullet y"
			);

			tests::Expect(
				result,
				IsNearlyEqual(bulletState.velocityX, 0.0F),
				"PlayerCommandService: fire velocity x"
			);

			tests::Expect(
				result,
				IsNearlyEqual(
					bulletState.velocityY,
					weaponRuleConfig.basicWeaponRule.bulletSpeed
				),
				"PlayerCommandService: fire velocity y"
			);

			tests::Expect(
				result,
				bulletState.damage ==
				weaponRuleConfig.basicWeaponRule.bulletDamage,
				"PlayerCommandService: fire damage"
			);

			tests::Expect(
				result,
				bulletState.radius ==
				weaponRuleConfig.basicWeaponRule.bulletRadius,
				"PlayerCommandService: fire radius"
			);
		}
	}

	void RunFireBulletCooldownRejectedTest(tests::DebugTestResult& result)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(6);
		const TimePoint joinTime = Clock::now();
		const TimePoint fireTime =
			joinTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(
			peerRoomManager,
			gameWorld,
			endpointKey,
			6,
			1,
			joinTime
		);

		server::game::PlayerState* playerState =
			gameWorld.FindPlayer(6);

		if (playerState != nullptr)
		{
			playerState->fireCooldownRemainingSeconds = 1.0F;
		}

		const bool fired = service.FireBullet(
			endpointKey,
			peerRoomManager,
			gameWorld,
			weaponRuleConfig,
			fireTime
		);

		tests::Expect(
			result,
			!fired,
			"PlayerCommandService: cooldown fire rejected"
		);

		tests::Expect(
			result,
			gameWorld.GetBulletCount() == 0,
			"PlayerCommandService: cooldown creates no bullet"
		);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastRecvTime == fireTime,
				"PlayerCommandService: cooldown still refreshes recv time"
			);
		}
	}

	void RunFireBulletDeadPlayerRejectedTest(tests::DebugTestResult& result)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(7);
		const TimePoint joinTime = Clock::now();
		const TimePoint fireTime =
			joinTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(
			peerRoomManager,
			gameWorld,
			endpointKey,
			7,
			1,
			joinTime
		);

		server::game::PlayerState* playerState =
			gameWorld.FindPlayer(7);

		if (playerState != nullptr)
		{
			playerState->isDead = true;
		}

		const bool fired = service.FireBullet(
			endpointKey,
			peerRoomManager,
			gameWorld,
			weaponRuleConfig,
			fireTime
		);

		tests::Expect(
			result,
			!fired,
			"PlayerCommandService: dead player fire rejected"
		);

		tests::Expect(
			result,
			gameWorld.GetBulletCount() == 0,
			"PlayerCommandService: dead player creates no bullet"
		);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastRecvTime == joinTime,
				"PlayerCommandService: dead fire recv time unchanged"
			);
		}
	}

	void RunFireBulletUnknownPeerRejectedTest(tests::DebugTestResult& result)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const bool fired = service.FireBullet(
			MakeEndpointKey(8),
			peerRoomManager,
			gameWorld,
			weaponRuleConfig,
			Clock::now()
		);

		tests::Expect(
			result,
			!fired,
			"PlayerCommandService: unknown peer fire rejected"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 0,
			"PlayerCommandService: unknown fire peer count"
		);

		tests::Expect(
			result,
			gameWorld.GetBulletCount() == 0,
			"PlayerCommandService: unknown fire bullet count"
		);
	}

	void RunFireBulletMissingPlayerRejectedTest(tests::DebugTestResult& result)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(9);
		const TimePoint joinTime = Clock::now();
		const TimePoint fireTime =
			joinTime + std::chrono::seconds(1);

		static_cast<void>(
			peerRoomManager.UpsertJoinedPeer(
				endpointKey,
				9,
				1,
				joinTime
			)
			);

		const bool fired = service.FireBullet(
			endpointKey,
			peerRoomManager,
			gameWorld,
			weaponRuleConfig,
			fireTime
		);

		tests::Expect(
			result,
			!fired,
			"PlayerCommandService: missing player fire rejected"
		);

		tests::Expect(
			result,
			gameWorld.GetBulletCount() == 0,
			"PlayerCommandService: missing player bullet count"
		);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastRecvTime == joinTime,
				"PlayerCommandService: missing player recv time unchanged"
			);
		}
	}
}

namespace tests::server
{
	DebugTestResult RunPlayerCommandServiceTests()
	{
		DebugTestResult result{};

		RunApplyInputCommandSuccessTest(result);
		RunApplyInputCommandStaleSequenceRejectedTest(result);
		RunApplyInputCommandWrappedSequenceAcceptedTest(result);
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