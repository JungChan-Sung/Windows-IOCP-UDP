#include "PeerSessionServiceTests.h"

#include <chrono>
#include <cstdint>

#include <Common/Game/GameRules.h>
#include <Common/Game/InputFlags.h>
#include <Common/Net/EndpointKey.h>
#include <Common/Net/SessionToken.h>

#include <Server/Game/BulletState.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/PlayerState.h>
#include <Server/Service/AuthenticatedAccountRegistry.h>
#include <Server/Service/PeerRoomManager.h>
#include <Server/Service/PeerSessionService.h>
#include <Server/Service/PeerState.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	inline constexpr std::int64_t testPersistentPlayerId = 5001;
	inline constexpr std::int64_t otherPersistentPlayerId = 5002;

	inline constexpr common::net::SessionToken testSessionToken{
		.high = 0x1122334455667788ULL,
		.low = 0x8877665544332211ULL,
	};

	inline constexpr common::net::SessionToken otherSessionToken{
		.high = 0x1234567890ABCDEFULL,
		.low = 0xFEDCBA0987654321ULL,
	};

	[[nodiscard]] constexpr common::net::EndpointKey MakeEndpointKey(std::uint32_t index) noexcept
	{
		return common::net::EndpointKey{
			.address = 0x7F000001 + index,
			.port = static_cast<std::uint16_t>(10000 + index),
		};
	}

	[[nodiscard]] server::service::PeerSessionService::AuthenticatedIdentity MakeAuthenticatedIdentity() noexcept
	{
		return server::service::PeerSessionService::AuthenticatedIdentity{
			.accountId = 1001,
			.persistentPlayerId = testPersistentPlayerId,
			.sessionToken = testSessionToken,
			.nickname = "nickname",
		};
	}

	[[nodiscard]] server::service::PeerSessionService::JoinResult JoinPeerForTest(
		const server::service::PeerSessionService& service,
		const common::net::EndpointKey& endpointKey,
		common::game::RoomId initialRoomId,
		server::service::PeerRoomManager& peerRoomManager,
		server::game::GameWorld& gameWorld,
		const common::game::GameRuleConfig& gameRuleConfig,
		TimePoint currentTime
	)
	{
		return service.JoinPeer(
			endpointKey,
			MakeAuthenticatedIdentity(),
			initialRoomId,
			peerRoomManager,
			gameWorld,
			gameRuleConfig,
			currentTime
		);
	}

	[[nodiscard]] server::game::PlayerState* FindPlayer(
		server::game::GameWorld& gameWorld,
		common::game::PlayerId playerId
	) noexcept
	{
		return gameWorld.FindPlayer(playerId);
	}

	void RunJoinPeerCreatesPeerAndPlayerTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(1);
		const TimePoint now = Clock::now();

		const server::service::PeerSessionService::JoinResult joinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, now);

		tests::Expect(result, joinResult.shouldSendResponse, "PeerSessionService: join sends response");
		tests::Expect(result, joinResult.shouldBroadcastPlayerJoined, "PeerSessionService: join broadcasts joined");
		tests::Expect(result, joinResult.playerId == 1, "PeerSessionService: join allocates first player id");
		tests::Expect(result, joinResult.persistentPlayerId == testPersistentPlayerId, "PeerSessionService: join persistent player id");
		tests::Expect(result, joinResult.roomId == 1, "PeerSessionService: join room id");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 1, "PeerSessionService: join peer count");
		tests::Expect(result, peerRoomManager.GetJoinedPeerCount() == 1, "PeerSessionService: join joined peer count");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(1) == 1, "PeerSessionService: join room count");
		tests::Expect(result, gameWorld.GetPlayerCount() == 1, "PeerSessionService: join player count");

		const server::service::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(result, peerState != nullptr, "PeerSessionService: join peer exists");

		if (peerState != nullptr)
		{
			tests::Expect(result, peerState->endpointKey == endpointKey, "PeerSessionService: join endpoint key");
			tests::Expect(result, peerState->accountId == 1001, "PeerSessionService: join account id");
			tests::Expect(result, peerState->persistentPlayerId == testPersistentPlayerId, "PeerSessionService: join persistent player id stored");
			tests::Expect(result, peerState->sessionToken == testSessionToken, "PeerSessionService: join session token");
			tests::Expect(result, peerState->nickname == "nickname", "PeerSessionService: join nickname");
			tests::Expect(result, peerState->playerId == joinResult.playerId, "PeerSessionService: join peer player id");
			tests::Expect(result, peerState->roomId == 1, "PeerSessionService: join peer room id");
			tests::Expect(result, peerState->lastRecvTime == now, "PeerSessionService: join last recv time");

			const server::service::PeerState* accountPeerState = peerRoomManager.FindJoinedPeerByAccountId(1001);

			tests::Expect(result, accountPeerState == peerState, "PeerRoomManager: account id returns joined peer");
			tests::Expect(result, peerRoomManager.FindJoinedPeerByAccountId(9999) == nullptr, "PeerRoomManager: unknown account id missing");
			tests::Expect(result, peerRoomManager.FindJoinedPeerByAccountId(0) == nullptr, "PeerRoomManager: invalid account id missing");
		}

		const server::game::PlayerState* playerState = gameWorld.FindPlayer(joinResult.playerId);

		tests::Expect(result, playerState != nullptr, "PeerSessionService: join player exists");

		if (playerState != nullptr)
		{
			tests::Expect(result, playerState->playerId == joinResult.playerId, "PeerSessionService: join player id");
			tests::Expect(result, playerState->x == joinResult.spawnPosition.x, "PeerSessionService: join player x");
			tests::Expect(result, playerState->y == joinResult.spawnPosition.y, "PeerSessionService: join player y");
			tests::Expect(result, playerState->hp == gameRuleConfig.initialPlayerHp, "PeerSessionService: join player hp");
			tests::Expect(result, !playerState->isDead, "PeerSessionService: join player alive");
		}
	}

	void RunJoinExistingPeerReturnsExistingPlayerTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(2);
		const TimePoint firstTime = Clock::now();
		const TimePoint secondTime = firstTime + std::chrono::seconds(5);

		const server::service::PeerSessionService::JoinResult firstJoinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, firstTime);

		const server::service::PeerSessionService::JoinResult secondJoinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, secondTime);

		tests::Expect(result, firstJoinResult.shouldBroadcastPlayerJoined, "PeerSessionService: existing join first broadcast");
		tests::Expect(result, secondJoinResult.shouldSendResponse, "PeerSessionService: existing join sends response");
		tests::Expect(result, !secondJoinResult.shouldBroadcastPlayerJoined, "PeerSessionService: existing join no broadcast");
		tests::Expect(result, secondJoinResult.playerId == firstJoinResult.playerId, "PeerSessionService: existing join same player");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 1, "PeerSessionService: existing join peer count");
		tests::Expect(result, gameWorld.GetPlayerCount() == 1, "PeerSessionService: existing join player count");

		const server::service::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(result, peerState != nullptr, "PeerSessionService: existing join peer exists");

		if (peerState != nullptr)
		{
			tests::Expect(result, peerState->persistentPlayerId == testPersistentPlayerId, "PeerSessionService: existing join identity preserved");
			tests::Expect(result, peerState->lastRecvTime == secondTime, "PeerSessionService: existing join refresh time");
		}
	}

	void RunJoinExistingPeerReturnsCurrentStateTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(11);
		const TimePoint joinTime = Clock::now();
		const TimePoint roomChangeTime = joinTime + std::chrono::seconds(1);
		const TimePoint retryTime = roomChangeTime + std::chrono::seconds(5);

		const server::service::PeerSessionService::JoinResult firstJoinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, joinTime);

		const server::service::PeerSessionService::RoomChangeResult roomChangeResult =
			service.ChangePeerRoom(endpointKey, 2, peerRoomManager, gameWorld, roomChangeTime);

		tests::Expect(result, roomChangeResult.changed, "PeerSessionService: existing join current state room change succeeds");

		const server::game::PlayerState* playerState = gameWorld.FindPlayer(firstJoinResult.playerId);

		tests::Expect(result, playerState != nullptr, "PeerSessionService: existing join current player exists");

		if (playerState == nullptr)
		{
			return;
		}

		const float currentX = playerState->x;
		const float currentY = playerState->y;

		const server::service::PeerSessionService::JoinResult retryJoinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, retryTime);

		tests::Expect(result, retryJoinResult.shouldSendResponse, "PeerSessionService: existing join current state sends response");
		tests::Expect(result, !retryJoinResult.shouldBroadcastPlayerJoined, "PeerSessionService: existing join current state no broadcast");
		tests::Expect(result, retryJoinResult.playerId == firstJoinResult.playerId, "PeerSessionService: existing join preserves player id");
		tests::Expect(result, retryJoinResult.roomId == 2, "PeerSessionService: existing join returns current room");
		tests::Expect(result, retryJoinResult.spawnPosition.x == currentX, "PeerSessionService: existing join returns current x");
		tests::Expect(result, retryJoinResult.spawnPosition.y == currentY, "PeerSessionService: existing join returns current y");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(1) == 0, "PeerSessionService: existing join initial room remains empty");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(2) == 1, "PeerSessionService: existing join current room count");
		tests::Expect(result, gameWorld.GetPlayerCount() == 1, "PeerSessionService: existing join current state player count");

		const server::service::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);

		if (peerState != nullptr)
		{
			tests::Expect(result, peerState->roomId == 2, "PeerSessionService: existing join preserves current peer room");
			tests::Expect(result, peerState->lastRecvTime == retryTime, "PeerSessionService: existing join current state refresh time");
		}
	}

	void RunLeavePeerRemovesPeerAndPlayerTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(3);

		const server::service::PeerSessionService::JoinResult joinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, Clock::now());

		gameWorld.AddBullet(server::game::BulletState{
			.bulletId = gameWorld.AllocateBulletId(),
			.roomId = 1,
			});

		const server::service::PeerSessionService::LeaveResult leaveResult =
			service.LeavePeer(endpointKey, peerRoomManager, gameWorld);

		tests::Expect(result, leaveResult.shouldBroadcastPlayerLeft, "PeerSessionService: leave broadcasts left");
		tests::Expect(result, leaveResult.playerId == joinResult.playerId, "PeerSessionService: leave player id");
		tests::Expect(result, leaveResult.persistentPlayerId == testPersistentPlayerId, "PeerSessionService: leave persistent player id");
		tests::Expect(result, leaveResult.roomId == 1, "PeerSessionService: leave room id");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: leave peer count");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(1) == 0, "PeerSessionService: leave room count");
		tests::Expect(result, gameWorld.GetPlayerCount() == 0, "PeerSessionService: leave player count");
		tests::Expect(result, gameWorld.FindPlayer(joinResult.playerId) == nullptr, "PeerSessionService: leave player removed");
		tests::Expect(result, gameWorld.GetBulletCount() == 0, "PeerSessionService: leave clears empty room transient state");
	}

	void RunLeaveUnknownPeerDoesNothingTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const server::service::PeerSessionService::LeaveResult leaveResult =
			service.LeavePeer(MakeEndpointKey(4), peerRoomManager, gameWorld);

		tests::Expect(result, !leaveResult.shouldBroadcastPlayerLeft, "PeerSessionService: unknown leave no broadcast");
		tests::Expect(result, leaveResult.playerId == 0, "PeerSessionService: unknown leave player id");
		tests::Expect(result, leaveResult.roomId == 0, "PeerSessionService: unknown leave room id");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: unknown leave peer count");
		tests::Expect(result, gameWorld.GetPlayerCount() == 0, "PeerSessionService: unknown leave player count");
	}

	void RunChangePeerRoomTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(5);
		const TimePoint joinTime = Clock::now();
		const TimePoint changeTime = joinTime + std::chrono::seconds(1);

		const server::service::PeerSessionService::JoinResult joinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, joinTime);

		server::game::PlayerState* playerState = FindPlayer(gameWorld, joinResult.playerId);
		if (playerState != nullptr)
		{
			playerState->inputFlags = common::game::InputFlags::Up;
		}

		gameWorld.AddBullet(server::game::BulletState{
			.bulletId = gameWorld.AllocateBulletId(),
			.roomId = 1,
			});

		const server::service::PeerSessionService::RoomChangeResult changeResult =
			service.ChangePeerRoom(endpointKey, 2, peerRoomManager, gameWorld, changeTime);

		tests::Expect(result, changeResult.changed, "PeerSessionService: room change succeeds");
		tests::Expect(result, changeResult.playerId == joinResult.playerId, "PeerSessionService: room change player id");
		tests::Expect(result, changeResult.persistentPlayerId == testPersistentPlayerId, "PeerSessionService: room change persistent player id");
		tests::Expect(result, changeResult.previousRoomId == 1, "PeerSessionService: room change previous room");
		tests::Expect(result, changeResult.nextRoomId == 2, "PeerSessionService: room change next room");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(1) == 0, "PeerSessionService: previous room empty");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(2) == 1, "PeerSessionService: next room count");
		tests::Expect(result, gameWorld.GetBulletCount() == 0, "PeerSessionService: room change clears previous empty room transient state");

		const server::service::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);

		if (peerState != nullptr)
		{
			tests::Expect(result, peerState->roomId == 2, "PeerSessionService: peer room changed");
			tests::Expect(result, peerState->lastRecvTime == changeTime, "PeerSessionService: room change recv time");
		}

		playerState = FindPlayer(gameWorld, joinResult.playerId);

		if (playerState != nullptr)
		{
			tests::Expect(result, playerState->x == changeResult.spawnPosition.x, "PeerSessionService: room change player x");
			tests::Expect(result, playerState->y == changeResult.spawnPosition.y, "PeerSessionService: room change player y");
			tests::Expect(result, playerState->inputFlags == common::game::InputFlags::None, "PeerSessionService: room change clears input");
		}
	}

	void RunChangePeerRoomInvalidRoomFailsTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(6);

		static_cast<void>(JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, Clock::now()));

		const server::service::PeerSessionService::RoomChangeResult resultValue =
			service.ChangePeerRoom(endpointKey, 0, peerRoomManager, gameWorld, Clock::now());

		tests::Expect(result, !resultValue.changed, "PeerSessionService: invalid room change fails");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(1) == 1, "PeerSessionService: invalid room original room remains");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(0) == 0, "PeerSessionService: invalid room not created");
	}

	void RunChangePeerRoomSameRoomFailsTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(7);

		static_cast<void>(JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, Clock::now()));

		const server::service::PeerSessionService::RoomChangeResult changeResult =
			service.ChangePeerRoom(endpointKey, 1, peerRoomManager, gameWorld, Clock::now());

		tests::Expect(result, !changeResult.changed, "PeerSessionService: same room change fails");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(1) == 1, "PeerSessionService: same room member count");
	}

	void RunChangePeerRoomUnknownPeerFailsTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const server::service::PeerSessionService::RoomChangeResult changeResult =
			service.ChangePeerRoom(MakeEndpointKey(8), 2, peerRoomManager, gameWorld, Clock::now());

		tests::Expect(result, !changeResult.changed, "PeerSessionService: unknown room change fails");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: unknown room change peer count");
		tests::Expect(result, gameWorld.GetPlayerCount() == 0, "PeerSessionService: unknown room change player count");
	}

	void RunChangePeerRoomDeadPlayerFailsTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(9);

		const server::service::PeerSessionService::JoinResult joinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, Clock::now());

		server::game::PlayerState* playerState = FindPlayer(gameWorld, joinResult.playerId);
		if (playerState != nullptr)
		{
			playerState->isDead = true;
		}

		const server::service::PeerSessionService::RoomChangeResult changeResult =
			service.ChangePeerRoom(endpointKey, 2, peerRoomManager, gameWorld, Clock::now());

		tests::Expect(result, !changeResult.changed, "PeerSessionService: dead player room change fails");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(1) == 1, "PeerSessionService: dead player original room remains");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(2) == 0, "PeerSessionService: dead player next room empty");
	}

	void RunJoinPeerRejectsInvalidIdentityTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const server::service::PeerSessionService::AuthenticatedIdentity invalidIdentity{
			.accountId = 0,
			.persistentPlayerId = testPersistentPlayerId,
			.sessionToken = testSessionToken,
			.nickname = "nickname",
		};

		const server::service::PeerSessionService::JoinResult joinResult =
			service.JoinPeer(MakeEndpointKey(12), invalidIdentity, 1, peerRoomManager, gameWorld, gameRuleConfig, Clock::now());

		tests::Expect(result, !joinResult.shouldSendResponse, "PeerSessionService: invalid identity no response");
		tests::Expect(result, !joinResult.shouldBroadcastPlayerJoined, "PeerSessionService: invalid identity no broadcast");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: invalid identity no peer");
		tests::Expect(result, gameWorld.GetPlayerCount() == 0, "PeerSessionService: invalid identity no player");
	}

	void RunJoinPeerRejectsInvalidPersistentPlayerIdTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const server::service::PeerSessionService::AuthenticatedIdentity invalidIdentity{
			.accountId = 1001,
			.persistentPlayerId = 0,
			.sessionToken = testSessionToken,
			.nickname = "nickname",
		};

		const server::service::PeerSessionService::JoinResult joinResult =
			service.JoinPeer(MakeEndpointKey(15), invalidIdentity, 1, peerRoomManager, gameWorld, gameRuleConfig, Clock::now());

		tests::Expect(result, !joinResult.shouldSendResponse, "PeerSessionService: invalid persistent player id no response");
		tests::Expect(result, !joinResult.shouldBroadcastPlayerJoined, "PeerSessionService: invalid persistent player id no broadcast");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: invalid persistent player id no peer");
		tests::Expect(result, gameWorld.GetPlayerCount() == 0, "PeerSessionService: invalid persistent player id no player");
	}

	void RunJoinPeerRejectsInvalidSessionTokenTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const server::service::PeerSessionService::AuthenticatedIdentity invalidIdentity{
			.accountId = 1001,
			.persistentPlayerId = testPersistentPlayerId,
			.sessionToken = common::net::invalidSessionToken,
			.nickname = "nickname",
		};

		const server::service::PeerSessionService::JoinResult joinResult =
			service.JoinPeer(MakeEndpointKey(13), invalidIdentity, 1, peerRoomManager, gameWorld, gameRuleConfig, Clock::now());

		tests::Expect(result, !joinResult.shouldSendResponse, "PeerSessionService: invalid token no response");
		tests::Expect(result, !joinResult.shouldBroadcastPlayerJoined, "PeerSessionService: invalid token no broadcast");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: invalid token no peer");
		tests::Expect(result, gameWorld.GetPlayerCount() == 0, "PeerSessionService: invalid token no player");
	}

	void RunJoinExistingPeerRejectsDifferentPersistentPlayerIdTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(16);
		const TimePoint firstJoinTime = Clock::now();

		const server::service::PeerSessionService::JoinResult firstJoinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, firstJoinTime);

		const server::service::PeerSessionService::AuthenticatedIdentity differentIdentity{
			.accountId = 1001,
			.persistentPlayerId = otherPersistentPlayerId,
			.sessionToken = testSessionToken,
			.nickname = "nickname",
		};

		const server::service::PeerSessionService::JoinResult retryJoinResult =
			service.JoinPeer(
				endpointKey,
				differentIdentity,
				1,
				peerRoomManager,
				gameWorld,
				gameRuleConfig,
				firstJoinTime + std::chrono::seconds(1)
			);

		tests::Expect(result, firstJoinResult.shouldSendResponse, "PeerSessionService: persistent player mismatch setup joined");
		tests::Expect(result, !retryJoinResult.shouldSendResponse, "PeerSessionService: different persistent player id rejected");
		tests::Expect(result, !retryJoinResult.shouldBroadcastPlayerJoined, "PeerSessionService: persistent player mismatch no broadcast");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 1, "PeerSessionService: persistent player mismatch preserves peer");
		tests::Expect(result, gameWorld.GetPlayerCount() == 1, "PeerSessionService: persistent player mismatch preserves player");

		const server::service::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr && peerState->persistentPlayerId == testPersistentPlayerId,
			"PeerSessionService: persistent player mismatch does not replace identity"
		);
		tests::Expect(
			result,
			peerState != nullptr && peerState->lastRecvTime == firstJoinTime,
			"PeerSessionService: persistent player mismatch does not refresh receive time"
		);
	}

	void RunJoinExistingPeerRejectsDifferentSessionTokenTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(14);
		const TimePoint firstJoinTime = Clock::now();

		const server::service::PeerSessionService::JoinResult firstJoinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, firstJoinTime);

		const server::service::PeerSessionService::AuthenticatedIdentity differentIdentity{
			.accountId = 1001,
			.persistentPlayerId = testPersistentPlayerId,
			.sessionToken = otherSessionToken,
			.nickname = "nickname",
		};

		const server::service::PeerSessionService::JoinResult retryJoinResult =
			service.JoinPeer(
				endpointKey,
				differentIdentity,
				1,
				peerRoomManager,
				gameWorld,
				gameRuleConfig,
				firstJoinTime + std::chrono::seconds(1)
			);

		tests::Expect(result, firstJoinResult.shouldSendResponse, "PeerSessionService: token mismatch setup joined");
		tests::Expect(result, !retryJoinResult.shouldSendResponse, "PeerSessionService: different existing token rejected");
		tests::Expect(result, !retryJoinResult.shouldBroadcastPlayerJoined, "PeerSessionService: different token no broadcast");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 1, "PeerSessionService: different token preserves existing peer");
		tests::Expect(result, gameWorld.GetPlayerCount() == 1, "PeerSessionService: different token preserves existing player");

		const server::service::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr && peerState->persistentPlayerId == testPersistentPlayerId && peerState->sessionToken == testSessionToken,
			"PeerSessionService: different token does not replace session"
		);
		tests::Expect(
			result,
			peerState != nullptr && peerState->lastRecvTime == firstJoinTime,
			"PeerSessionService: rejected token does not refresh receive time"
		);
	}

	void RunRemoveTimedOutPeersTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(20);
		const TimePoint joinTime = Clock::now();

		const server::service::PeerSessionService::JoinResult joinResult =
			JoinPeerForTest(service, endpointKey, 1, peerRoomManager, gameWorld, gameRuleConfig, joinTime);

		gameWorld.AddBullet(server::game::BulletState{
			.bulletId = gameWorld.AllocateBulletId(),
			.roomId = 1,
			});

		const server::service::PeerSessionService::TimedOutPeerList timedOutPeerList = service.RemoveTimedOutPeers(
			joinTime + std::chrono::seconds(10),
			std::chrono::seconds(5),
			peerRoomManager,
			gameWorld
		);

		tests::Expect(result, timedOutPeerList.size() == 1, "PeerSessionService: timed out peer removed");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: timeout removes peer");
		tests::Expect(result, gameWorld.FindPlayer(joinResult.playerId) == nullptr, "PeerSessionService: timeout removes player");
		tests::Expect(result, gameWorld.GetBulletCount() == 0, "PeerSessionService: timeout clears empty room transient state");

		if (!timedOutPeerList.empty())
		{
			tests::Expect(result, timedOutPeerList.front().endpointKey == endpointKey, "PeerSessionService: timeout endpoint");
			tests::Expect(result, timedOutPeerList.front().playerId == joinResult.playerId, "PeerSessionService: timeout player id");
			tests::Expect(result, timedOutPeerList.front().persistentPlayerId == testPersistentPlayerId,
				"PeerSessionService: timeout persistent player id");
			tests::Expect(result, timedOutPeerList.front().roomId == 1, "PeerSessionService: timeout room id");
		}
	}

	void RunJoinAuthenticatedPeerTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::AuthenticatedAccountRegistry authenticatedAccountRegistry;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(21);
		const TimePoint now = Clock::now();

		const bool registered = authenticatedAccountRegistry.Upsert(
			endpointKey,
			1001,
			testPersistentPlayerId,
			testSessionToken,
			"nickname",
			now
		);

		tests::Expect(result, registered, "PeerSessionService: authenticated account registered");

		const server::service::PeerSessionService::JoinAuthenticatedPeerResult joinResult =
			service.JoinAuthenticatedPeer(
				endpointKey,
				testSessionToken,
				1,
				authenticatedAccountRegistry,
				peerRoomManager,
				gameWorld,
				gameRuleConfig,
				now
			);

		tests::Expect(
			result,
			joinResult.status == server::service::PeerSessionService::JoinAuthenticatedPeerStatus::Joined,
			"PeerSessionService: authenticated peer joined"
		);

		tests::Expect(result, joinResult.joinResult.shouldSendResponse, "PeerSessionService: authenticated join sends response");
		tests::Expect(result, joinResult.joinResult.shouldBroadcastPlayerJoined, "PeerSessionService: authenticated join broadcasts");
		tests::Expect(result, authenticatedAccountRegistry.GetCount() == 0, "PeerSessionService: authenticated account consumed");
		tests::Expect(result, peerRoomManager.GetJoinedPeerCount() == 1, "PeerSessionService: authenticated join creates peer");
		tests::Expect(result, gameWorld.GetPlayerCount() == 1, "PeerSessionService: authenticated join creates player");
	}

	void RunJoinAuthenticatedExistingPeerTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::AuthenticatedAccountRegistry authenticatedAccountRegistry;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(22);
		const TimePoint now = Clock::now();

		static_cast<void>(authenticatedAccountRegistry.Upsert(
			endpointKey,
			1001,
			testPersistentPlayerId,
			testSessionToken,
			"nickname",
			now
		));

		const server::service::PeerSessionService::JoinAuthenticatedPeerResult firstResult =
			service.JoinAuthenticatedPeer(
				endpointKey,
				testSessionToken,
				1,
				authenticatedAccountRegistry,
				peerRoomManager,
				gameWorld,
				gameRuleConfig,
				now
			);

		const server::service::PeerSessionService::JoinAuthenticatedPeerResult retryResult =
			service.JoinAuthenticatedPeer(
				endpointKey,
				testSessionToken,
				1,
				authenticatedAccountRegistry,
				peerRoomManager,
				gameWorld,
				gameRuleConfig,
				now + std::chrono::seconds(1)
			);

		tests::Expect(
			result,
			firstResult.status == server::service::PeerSessionService::JoinAuthenticatedPeerStatus::Joined,
			"PeerSessionService: authenticated existing setup joined"
		);

		tests::Expect(
			result,
			retryResult.status == server::service::PeerSessionService::JoinAuthenticatedPeerStatus::ExistingPeer,
			"PeerSessionService: authenticated existing peer detected"
		);

		tests::Expect(result, retryResult.joinResult.shouldSendResponse, "PeerSessionService: existing peer retry sends response");
		tests::Expect(result, !retryResult.joinResult.shouldBroadcastPlayerJoined, "PeerSessionService: existing peer retry no broadcast");
	}

	void RunJoinAuthenticatedPeerRejectsUnauthenticatedTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::AuthenticatedAccountRegistry authenticatedAccountRegistry;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const server::service::PeerSessionService::JoinAuthenticatedPeerResult joinResult =
			service.JoinAuthenticatedPeer(
				MakeEndpointKey(23),
				testSessionToken,
				1,
				authenticatedAccountRegistry,
				peerRoomManager,
				gameWorld,
				gameRuleConfig,
				Clock::now()
			);

		tests::Expect(
			result,
			joinResult.status == server::service::PeerSessionService::JoinAuthenticatedPeerStatus::Unauthenticated,
			"PeerSessionService: unauthenticated peer rejected"
		);

		tests::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: unauthenticated join creates no peer");
		tests::Expect(result, gameWorld.GetPlayerCount() == 0, "PeerSessionService: unauthenticated join creates no player");
	}

	void RunJoinAuthenticatedExistingPeerRejectsTokenMismatchTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::AuthenticatedAccountRegistry authenticatedAccountRegistry;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey endpointKey = MakeEndpointKey(24);
		const TimePoint now = Clock::now();

		static_cast<void>(authenticatedAccountRegistry.Upsert(
			endpointKey,
			1001,
			testPersistentPlayerId,
			testSessionToken,
			"nickname",
			now
		));

		static_cast<void>(service.JoinAuthenticatedPeer(
			endpointKey,
			testSessionToken,
			1,
			authenticatedAccountRegistry,
			peerRoomManager,
			gameWorld,
			gameRuleConfig,
			now
		));

		const server::service::PeerSessionService::JoinAuthenticatedPeerResult retryResult =
			service.JoinAuthenticatedPeer(
				endpointKey,
				otherSessionToken,
				1,
				authenticatedAccountRegistry,
				peerRoomManager,
				gameWorld,
				gameRuleConfig,
				now + std::chrono::seconds(1)
			);

		tests::Expect(
			result,
			retryResult.status == server::service::PeerSessionService::JoinAuthenticatedPeerStatus::Rejected,
			"PeerSessionService: existing peer token mismatch rejected"
		);
	}

	void RunTimedOutPeerBecomesRecoverableTest(tests::DebugTestResult& result)
	{
		server::service::PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(1);

		const TimePoint joinTime = Clock::now();

		server::service::PeerState& peerState =
			peerRoomManager.UpsertJoinedPeer(
				endpointKey,
				1,
				1,
				joinTime
			);

		peerState.persistentPlayerId = 5001;

		const auto recoverablePeerList =
			peerRoomManager.MarkTimedOutPeersRecoverable(
				joinTime + std::chrono::seconds(10),
				std::chrono::seconds(5)
			);

		tests::Expect(
			result,
			recoverablePeerList.size() == 1,
			"PeerRoomManager: timed out peer becomes recoverable"
		);

		const server::service::PeerState* recoverablePeer =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			recoverablePeer != nullptr,
			"PeerRoomManager: recoverable peer remains joined"
		);

		if (recoverablePeer != nullptr)
		{
			tests::Expect(
				result,
				recoverablePeer->connectionState
				== server::service::PeerConnectionState::Recoverable,
				"PeerRoomManager: recoverable peer connection state"
			);
		}
	}
}

namespace tests::server
{
	DebugTestResult RunPeerSessionServiceTests()
	{
		DebugTestResult result{};

		RunJoinPeerCreatesPeerAndPlayerTest(result);
		RunJoinPeerRejectsInvalidIdentityTest(result);
		RunJoinPeerRejectsInvalidPersistentPlayerIdTest(result);
		RunJoinPeerRejectsInvalidSessionTokenTest(result);
		RunJoinExistingPeerRejectsDifferentPersistentPlayerIdTest(result);
		RunJoinExistingPeerRejectsDifferentSessionTokenTest(result);
		RunJoinExistingPeerReturnsExistingPlayerTest(result);
		RunJoinExistingPeerReturnsCurrentStateTest(result);
		RunLeavePeerRemovesPeerAndPlayerTest(result);
		RunLeaveUnknownPeerDoesNothingTest(result);
		RunChangePeerRoomTest(result);
		RunChangePeerRoomInvalidRoomFailsTest(result);
		RunChangePeerRoomSameRoomFailsTest(result);
		RunChangePeerRoomUnknownPeerFailsTest(result);
		RunChangePeerRoomDeadPlayerFailsTest(result);
		RunRemoveTimedOutPeersTest(result);
		RunJoinAuthenticatedPeerTest(result);
		RunJoinAuthenticatedExistingPeerTest(result);
		RunJoinAuthenticatedPeerRejectsUnauthenticatedTest(result);
		RunJoinAuthenticatedExistingPeerRejectsTokenMismatchTest(result);

		return result;
	}
}