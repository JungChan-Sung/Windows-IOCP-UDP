#include "PlayerCommandServiceTests.h"

#include <WinSock2.h>

#include <chrono>
#include <cmath>
#include <cstdint>

#include <Common/Game/InputFlags.h>
#include <Common/Game/WeaponRules.h>
#include <Common/Net/Endpoint.h>

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

	using PeerRoomManager =
		server::service::PeerRoomManager;

	using PeerState =
		server::service::PeerState;

	using PlayerCommandService =
		server::service::PlayerCommandService;

	[[nodiscard]] bool IsNearlyEqual(
		float lhs,
		float rhs,
		float epsilon = 0.001F
	) noexcept
	{
		return std::fabs(lhs - rhs) <= epsilon;
	}

	[[nodiscard]] common::net::EndpointKey MakeEndpointKey(
		std::uint32_t index
	) noexcept
	{
		common::net::EndpointKey endpointKey{};
		endpointKey.address = 0x7F000001 + index;
		endpointKey.port =
			static_cast<std::uint16_t>(10000 + index);
		return endpointKey;
	}

	[[nodiscard]] sockaddr_in MakeRemoteAddress(
		const common::net::EndpointKey& endpointKey
	) noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_addr.S_un.S_addr =
			endpointKey.address;
		remoteAddress.sin_port = endpointKey.port;
		return remoteAddress;
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
		playerState.hp =
			common::game::defaultInitialPlayerHp;
		playerState.isDead = false;
		playerState.weaponType =
			common::game::WeaponType::Basic;
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
		const sockaddr_in remoteAddress =
			MakeRemoteAddress(endpointKey);

		PeerState& peerState =
			peerRoomManager.UpsertJoinedPeer(
				remoteAddress,
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

	void RunApplyInputCommandSuccessTest(
		tests::DebugTestResult& result
	)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(1);

		constexpr common::game::PlayerId playerId = 1;
		constexpr common::game::RoomId roomId = 1;

		const TimePoint joinTime = Clock::now();
		const TimePoint commandTime =
			joinTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(
			peerRoomManager,
			gameWorld,
			endpointKey,
			playerId,
			roomId,
			joinTime
		);

		constexpr std::uint32_t inputSequence = 1;
		const common::game::InputFlags inputFlags = common::game::InputFlags::Right | common::game::InputFlags::Up;

		const bool applied = service.ApplyInputCommand(endpointKey, inputSequence, inputFlags, peerRoomManager, gameWorld, commandTime);

		tests::Expect(
			result,
			applied,
			"PlayerCommandService: input command applied"
		);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		const server::game::PlayerState* playerState =
			gameWorld.FindPlayer(playerId);

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
				peerState->lastInputSequence == 1,
				"PlayerCommandService: input sequence updated"
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

	void RunApplyInputCommandStaleSequenceRejectedTest(
		tests::DebugTestResult& result
	)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(2);

		constexpr common::game::PlayerId playerId = 2;
		constexpr common::game::RoomId roomId = 1;

		const TimePoint joinTime = Clock::now();

		const TimePoint firstCommandTime =
			joinTime + std::chrono::seconds(1);

		const TimePoint staleCommandTime =
			firstCommandTime + std::chrono::seconds(1);

		AddJoinedPeerAndPlayer(
			peerRoomManager,
			gameWorld,
			endpointKey,
			playerId,
			roomId,
			joinTime
		);

		constexpr std::uint32_t firstInputSequence = 10;
		constexpr common::game::InputFlags firstInputFlags = common::game::InputFlags::Right;

		constexpr std::uint32_t staleInputSequence = 10;
		constexpr common::game::InputFlags staleInputFlags = common::game::InputFlags::Left;

		const bool firstApplied = service.ApplyInputCommand(
			endpointKey,
			firstInputSequence,
			firstInputFlags,
			peerRoomManager,
			gameWorld,
			firstCommandTime
		);

		const bool staleApplied = service.ApplyInputCommand(
			endpointKey,
			staleInputSequence,
			staleInputFlags,
			peerRoomManager,
			gameWorld,
			staleCommandTime
		);
		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		const server::game::PlayerState* playerState =
			gameWorld.FindPlayer(playerId);

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

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastInputSequence == 10,
				"PlayerCommandService: stale sequence unchanged"
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
				playerState->inputFlags
				== common::game::InputFlags::Right,
				"PlayerCommandService: stale input flags unchanged"
			);
		}
	}

	void RunApplyInputCommandDeadPlayerRejectedTest(
		tests::DebugTestResult& result
	)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(3);

		constexpr common::game::PlayerId playerId = 3;
		constexpr common::game::RoomId roomId = 1;

		const TimePoint joinTime = Clock::now();

		const TimePoint commandTime =
			joinTime + std::chrono::seconds(1);

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
			playerState->isDead = true;
		}

		constexpr std::uint32_t inputSequence = 1;
		constexpr common::game::InputFlags inputFlags = common::game::InputFlags::Down;

		const bool applied =
			service.ApplyInputCommand(
				endpointKey,
				inputSequence,
				inputFlags,
				peerRoomManager,
				gameWorld,
				commandTime
			);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			!applied,
			"PlayerCommandService: dead player input rejected"
		);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastInputSequence == 0,
				"PlayerCommandService: dead input sequence unchanged"
			);

			tests::Expect(
				result,
				peerState->lastRecvTime == joinTime,
				"PlayerCommandService: dead input recv time unchanged"
			);
		}
	}

	void RunApplyInputCommandUnknownPeerRejectedTest(
		tests::DebugTestResult& result
	)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(4);

		const TimePoint commandTime = Clock::now();

		constexpr std::uint32_t inputSequence = 1;
		constexpr common::game::InputFlags inputFlags = common::game::InputFlags::Right;

		const bool applied =
			service.ApplyInputCommand(
				endpointKey,
				inputSequence,
				inputFlags,
				peerRoomManager,
				gameWorld,
				commandTime
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

	void RunFireBulletSuccessTest(
		tests::DebugTestResult& result
	)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(5);

		constexpr common::game::PlayerId playerId = 5;
		constexpr common::game::RoomId roomId = 2;

		const TimePoint joinTime = Clock::now();

		const TimePoint fireTime =
			joinTime + std::chrono::seconds(1);

		constexpr std::int64_t persistentPlayerId =
			5000 + playerId;

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

		const bool fired =
			service.FireBullet(
				endpointKey,
				peerRoomManager,
				gameWorld,
				weaponRuleConfig,
				fireTime
			);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		const server::game::PlayerState* updatedPlayerState =
			gameWorld.FindPlayer(playerId);

		const server::game::GameWorld::BulletStateList&
			bulletStateList =
			gameWorld.GetBulletStateList();

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

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastRecvTime == fireTime,
				"PlayerCommandService: fire recv time updated"
			);
		}

		if (updatedPlayerState != nullptr)
		{
			tests::Expect(
				result,
				updatedPlayerState->fireCooldownRemainingSeconds
				== weaponRuleConfig.basicWeaponRule
				.fireCooldownSeconds,
				"PlayerCommandService: fire cooldown set"
			);
		}

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
				bulletState.ownerPersistentPlayerId
				== persistentPlayerId,
				"PlayerCommandService: fire persistent owner id"
			);

			tests::Expect(
				result,
				bulletState.roomId == roomId,
				"PlayerCommandService: fire room id"
			);

			tests::Expect(
				result,
				IsNearlyEqual(
					bulletState.x,
					50.0F
				),
				"PlayerCommandService: fire bullet x"
			);

			tests::Expect(
				result,
				IsNearlyEqual(
					bulletState.y,
					60.0F
				),
				"PlayerCommandService: fire bullet y"
			);

			tests::Expect(
				result,
				IsNearlyEqual(
					bulletState.velocityX,
					0.0F
				),
				"PlayerCommandService: fire velocity x"
			);

			tests::Expect(
				result,
				IsNearlyEqual(
					bulletState.velocityY,
					weaponRuleConfig.basicWeaponRule
					.bulletSpeed
				),
				"PlayerCommandService: fire velocity y"
			);

			tests::Expect(
				result,
				bulletState.damage
				== weaponRuleConfig.basicWeaponRule
				.bulletDamage,
				"PlayerCommandService: fire damage"
			);

			tests::Expect(
				result,
				bulletState.radius
				== weaponRuleConfig.basicWeaponRule
				.bulletRadius,
				"PlayerCommandService: fire radius"
			);
		}
	}

	void RunFireBulletCooldownRejectedTest(
		tests::DebugTestResult& result
	)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(6);

		constexpr common::game::PlayerId playerId = 6;
		constexpr common::game::RoomId roomId = 1;

		const TimePoint joinTime = Clock::now();

		const TimePoint fireTime =
			joinTime + std::chrono::seconds(1);

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
			playerState->fireCooldownRemainingSeconds = 1.0F;
		}

		const bool fired =
			service.FireBullet(
				endpointKey,
				peerRoomManager,
				gameWorld,
				weaponRuleConfig,
				fireTime
			);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

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

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastRecvTime == fireTime,
				"PlayerCommandService: cooldown still refreshes recv time"
			);
		}
	}

	void RunFireBulletDeadPlayerRejectedTest(
		tests::DebugTestResult& result
	)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(7);

		constexpr common::game::PlayerId playerId = 7;
		constexpr common::game::RoomId roomId = 1;

		const TimePoint joinTime = Clock::now();

		const TimePoint fireTime =
			joinTime + std::chrono::seconds(1);

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
			playerState->isDead = true;
		}

		const bool fired =
			service.FireBullet(
				endpointKey,
				peerRoomManager,
				gameWorld,
				weaponRuleConfig,
				fireTime
			);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

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

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->lastRecvTime == joinTime,
				"PlayerCommandService: dead fire recv time unchanged"
			);
		}
	}

	void RunFireBulletUnknownPeerRejectedTest(
		tests::DebugTestResult& result
	)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(8);

		const TimePoint fireTime = Clock::now();

		const bool fired =
			service.FireBullet(
				endpointKey,
				peerRoomManager,
				gameWorld,
				weaponRuleConfig,
				fireTime
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

	void RunFireBulletMissingPlayerRejectedTest(
		tests::DebugTestResult& result
	)
	{
		PlayerCommandService service;
		PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(9);

		constexpr common::game::PlayerId playerId = 9;
		constexpr common::game::RoomId roomId = 1;

		const TimePoint joinTime = Clock::now();

		const TimePoint fireTime =
			joinTime + std::chrono::seconds(1);

		const sockaddr_in remoteAddress =
			MakeRemoteAddress(endpointKey);

		peerRoomManager.UpsertJoinedPeer(
			remoteAddress,
			endpointKey,
			playerId,
			roomId,
			joinTime
		);

		const bool fired =
			service.FireBullet(
				endpointKey,
				peerRoomManager,
				gameWorld,
				weaponRuleConfig,
				fireTime
			);

		const PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

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
	tests::DebugTestResult RunPlayerCommandServiceTests()
	{
		tests::DebugTestResult result{};

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