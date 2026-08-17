#include "PeerSessionServiceTests.h"

#include <WinSock2.h>

#include <chrono>
#include <cstdint>
#include <string_view>

#include <Common/Game/InputFlags.h>
#include <Common/Net/Endpoint.h>
#include <Common/Net/Reliable/ReliableUdpConfig.h>
#include <Common/Net/SessionToken.h>
#include <Common/Game/GameRules.h>

#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/PlayerState.h>
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

	[[nodiscard]] sockaddr_in MakeRemoteAddress(std::uint32_t index) noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_addr.S_un.S_addr = ::htonl(0x7F000001 + index);
		remoteAddress.sin_port =
			::htons(static_cast<u_short>(10000 + index));
		return remoteAddress;
	}

	[[nodiscard]] common::net::EndpointKey MakeEndpointKey(
		const sockaddr_in& remoteAddress
	) noexcept
	{
		return common::net::MakeEndpointKey(remoteAddress);
	}

	[[nodiscard]]
	server::service::PeerSessionService::JoinResult JoinPeerForTest(
		const server::service::PeerSessionService& service,
		const sockaddr_in& remoteAddress,
		const common::net::EndpointKey& endpointKey,
		common::game::RoomId initialRoomId,
		server::service::PeerRoomManager& peerRoomManager,
		server::game::GameWorld& gameWorld,
		const server::game::GameSimulation& gameSimulation,
		const common::game::GameRuleConfig& gameRuleConfig,
		const common::net::ReliableUdpConfig& reliableUdpConfig,
		TimePoint currentTime
	)
	{
		const server::service::PeerSessionService::AuthenticatedIdentity
			authenticatedIdentity{
				.accountId = 1001,
				.persistentPlayerId = testPersistentPlayerId,
				.sessionToken = testSessionToken,
				.nickname = "nickname",
		};

		return service.JoinPeer(
			remoteAddress,
			endpointKey,
			authenticatedIdentity,
			initialRoomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			reliableUdpConfig,
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

	void RunJoinPeerCreatesPeerAndPlayerTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(1);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		constexpr common::game::RoomId initialRoomId = 1;
		const TimePoint now = Clock::now();

		const server::service::PeerSessionService::JoinResult joinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				initialRoomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				now
			);

		tests::Expect(
			result,
			joinResult.shouldSendResponse,
			"PeerSessionService: join sends response"
		);

		tests::Expect(
			result,
			joinResult.shouldBroadcastPlayerJoined,
			"PeerSessionService: join broadcasts joined"
		);

		tests::Expect(
			result,
			joinResult.playerId == 1,
			"PeerSessionService: join allocates first player id"
		);

		tests::Expect(
			result,
			joinResult.roomId == initialRoomId,
			"PeerSessionService: join room id"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 1,
			"PeerSessionService: join peer count"
		);

		tests::Expect(
			result,
			peerRoomManager.GetJoinedPeerCount() == 1,
			"PeerSessionService: join joined peer count"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(initialRoomId) == 1,
			"PeerSessionService: join room count"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 1,
			"PeerSessionService: join player count"
		);

		const server::service::PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr,
			"PeerSessionService: join peer exists"
		);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->accountId == 1001,
				"PeerSessionService: join account id"
			);

			tests::Expect(
				result,
				peerState->persistentPlayerId == testPersistentPlayerId,
				"PeerSessionService: join persistent player id"
			);

			tests::Expect(
				result,
				peerState->sessionToken == testSessionToken,
				"PeerSessionService: join session token"
			);

			tests::Expect(
				result,
				peerState->nickname == "nickname",
				"PeerSessionService: join nickname"
			);

			tests::Expect(
				result,
				peerState->playerId == joinResult.playerId,
				"PeerSessionService: join peer player id"
			);

			tests::Expect(
				result,
				peerState->roomId == initialRoomId,
				"PeerSessionService: join peer room id"
			);

			tests::Expect(
				result,
				peerState->lastRecvTime == now,
				"PeerSessionService: join lastRecvTime"
			);

			const server::service::PeerState* accountPeerState =
				peerRoomManager.FindJoinedPeerByAccountId(1001);

			tests::Expect(
				result,
				accountPeerState != nullptr,
				"PeerRoomManager: joined peer found by account id"
			);

			tests::Expect(
				result,
				accountPeerState == peerState,
				"PeerRoomManager: account id returns joined peer"
			);

			tests::Expect(
				result,
				peerRoomManager.FindJoinedPeerByAccountId(9999) == nullptr,
				"PeerRoomManager: unknown account id missing"
			);

			tests::Expect(
				result,
				peerRoomManager.FindJoinedPeerByAccountId(0) == nullptr,
				"PeerRoomManager: invalid account id missing"
			);
		}

		const server::game::PlayerState* playerState =
			gameWorld.FindPlayer(joinResult.playerId);

		tests::Expect(
			result,
			playerState != nullptr,
			"PeerSessionService: join player exists"
		);

		if (playerState != nullptr)
		{
			tests::Expect(
				result,
				playerState->playerId == joinResult.playerId,
				"PeerSessionService: join player id"
			);

			tests::Expect(
				result,
				playerState->x == joinResult.spawnPosition.x,
				"PeerSessionService: join player x"
			);

			tests::Expect(
				result,
				playerState->y == joinResult.spawnPosition.y,
				"PeerSessionService: join player y"
			);

			tests::Expect(
				result,
				playerState->hp == gameRuleConfig.initialPlayerHp,
				"PeerSessionService: join player hp"
			);

			tests::Expect(
				result,
				!playerState->isDead,
				"PeerSessionService: join player alive"
			);
		}
	}

	void RunJoinExistingPeerReturnsExistingPlayerTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(2);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		constexpr common::game::RoomId initialRoomId = 1;

		const TimePoint firstTime = Clock::now();
		const TimePoint secondTime =
			firstTime + std::chrono::seconds(5);

		const server::service::PeerSessionService::JoinResult firstJoinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				initialRoomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				firstTime
			);

		const server::service::PeerSessionService::JoinResult secondJoinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				initialRoomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				secondTime
			);

		tests::Expect(
			result,
			firstJoinResult.shouldBroadcastPlayerJoined,
			"PeerSessionService: existing join first broadcast"
		);

		tests::Expect(
			result,
			secondJoinResult.shouldSendResponse,
			"PeerSessionService: existing join sends response"
		);

		tests::Expect(
			result,
			!secondJoinResult.shouldBroadcastPlayerJoined,
			"PeerSessionService: existing join no broadcast"
		);

		tests::Expect(
			result,
			secondJoinResult.playerId == firstJoinResult.playerId,
			"PeerSessionService: existing join same player"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 1,
			"PeerSessionService: existing join peer count"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 1,
			"PeerSessionService: existing join player count"
		);

		const server::service::PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr,
			"PeerSessionService: existing join peer exists"
		);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->persistentPlayerId == testPersistentPlayerId,
				"PeerSessionService: existing join persistent player id preserved"
			);

			tests::Expect(
				result,
				peerState->lastRecvTime == secondTime,
				"PeerSessionService: existing join refresh time"
			);
		}
	}

	void RunJoinExistingPeerReturnsCurrentStateTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(11);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		constexpr common::game::RoomId initialRoomId = 1;
		constexpr common::game::RoomId nextRoomId = 2;

		const TimePoint joinTime = Clock::now();
		const TimePoint roomChangeTime =
			joinTime + std::chrono::seconds(1);

		const TimePoint retryTime =
			roomChangeTime + std::chrono::seconds(5);

		const server::service::PeerSessionService::JoinResult firstJoinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				initialRoomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				joinTime
			);

		const server::service::PeerSessionService::RoomChangeResult
			roomChangeResult =
			service.ChangePeerRoom(
				endpointKey,
				nextRoomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				roomChangeTime
			);

		tests::Expect(
			result,
			roomChangeResult.changed,
			"PeerSessionService: existing join current state room change succeeds"
		);

		const server::game::PlayerState* playerState =
			gameWorld.FindPlayer(firstJoinResult.playerId);

		tests::Expect(
			result,
			playerState != nullptr,
			"PeerSessionService: existing join current player exists"
		);

		if (playerState == nullptr)
		{
			return;
		}

		const float currentX = playerState->x;
		const float currentY = playerState->y;

		const server::service::PeerSessionService::JoinResult retryJoinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				initialRoomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				retryTime
			);

		tests::Expect(
			result,
			retryJoinResult.shouldSendResponse,
			"PeerSessionService: existing join current state sends response"
		);

		tests::Expect(
			result,
			!retryJoinResult.shouldBroadcastPlayerJoined,
			"PeerSessionService: existing join current state does not broadcast"
		);

		tests::Expect(
			result,
			retryJoinResult.playerId == firstJoinResult.playerId,
			"PeerSessionService: existing join current state preserves player id"
		);

		tests::Expect(
			result,
			retryJoinResult.roomId == nextRoomId,
			"PeerSessionService: existing join returns current room id"
		);

		tests::Expect(
			result,
			retryJoinResult.spawnPosition.x == currentX,
			"PeerSessionService: existing join returns current x"
		);

		tests::Expect(
			result,
			retryJoinResult.spawnPosition.y == currentY,
			"PeerSessionService: existing join returns current y"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 1,
			"PeerSessionService: existing join current state peer count"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(initialRoomId) == 0,
			"PeerSessionService: existing join initial room remains empty"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(nextRoomId) == 1,
			"PeerSessionService: existing join current room member count"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 1,
			"PeerSessionService: existing join current state player count"
		);

		const server::service::PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr,
			"PeerSessionService: existing join current peer exists"
		);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->persistentPlayerId == testPersistentPlayerId,
				"PeerSessionService: existing join current state persistent player id"
			);

			tests::Expect(
				result,
				peerState->roomId == nextRoomId,
				"PeerSessionService: existing join preserves current peer room"
			);

			tests::Expect(
				result,
				peerState->lastRecvTime == retryTime,
				"PeerSessionService: existing join current state refresh time"
			);
		}
	}

	void RunLeavePeerRemovesPeerAndPlayerTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(3);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		constexpr common::game::RoomId roomId = 1;
		const TimePoint now = Clock::now();

		const server::service::PeerSessionService::JoinResult joinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				roomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				now
			);

		const server::service::PeerSessionService::LeaveResult leaveResult =
			service.LeavePeer(
				endpointKey,
				peerRoomManager,
				gameWorld
			);

		tests::Expect(
			result,
			leaveResult.shouldBroadcastPlayerLeft,
			"PeerSessionService: leave broadcasts left"
		);

		tests::Expect(
			result,
			leaveResult.playerId == joinResult.playerId,
			"PeerSessionService: leave player id"
		);

		tests::Expect(
			result,
			leaveResult.roomId == roomId,
			"PeerSessionService: leave room id"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 0,
			"PeerSessionService: leave peer count"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(roomId) == 0,
			"PeerSessionService: leave room count"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 0,
			"PeerSessionService: leave player count"
		);

		tests::Expect(
			result,
			gameWorld.FindPlayer(joinResult.playerId) == nullptr,
			"PeerSessionService: leave player removed"
		);
	}

	void RunLeaveUnknownPeerDoesNothingTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const sockaddr_in remoteAddress = MakeRemoteAddress(4);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		const server::service::PeerSessionService::LeaveResult leaveResult =
			service.LeavePeer(
				endpointKey,
				peerRoomManager,
				gameWorld
			);

		tests::Expect(
			result,
			!leaveResult.shouldBroadcastPlayerLeft,
			"PeerSessionService: unknown leave no broadcast"
		);

		tests::Expect(
			result,
			leaveResult.playerId == 0,
			"PeerSessionService: unknown leave player id"
		);

		tests::Expect(
			result,
			leaveResult.roomId == 0,
			"PeerSessionService: unknown leave room id"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 0,
			"PeerSessionService: unknown leave peer count"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 0,
			"PeerSessionService: unknown leave player count"
		);
	}

	void RunChangePeerRoomTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(5);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		constexpr common::game::RoomId previousRoomId = 1;
		constexpr common::game::RoomId nextRoomId = 2;

		const TimePoint joinTime = Clock::now();
		const TimePoint changeTime =
			joinTime + std::chrono::seconds(1);

		const server::service::PeerSessionService::JoinResult joinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				previousRoomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				joinTime
			);

		server::game::PlayerState* playerState =
			FindPlayer(gameWorld, joinResult.playerId);

		if (playerState != nullptr)
		{
			playerState->inputFlags = common::game::InputFlags::Up;
		}

		const server::service::PeerSessionService::RoomChangeResult
			changeResult =
			service.ChangePeerRoom(
				endpointKey,
				nextRoomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				changeTime
			);

		tests::Expect(
			result,
			changeResult.changed,
			"PeerSessionService: room change succeeds"
		);

		tests::Expect(
			result,
			changeResult.playerId == joinResult.playerId,
			"PeerSessionService: room change player id"
		);

		tests::Expect(
			result,
			changeResult.previousRoomId == previousRoomId,
			"PeerSessionService: room change previous room"
		);

		tests::Expect(
			result,
			changeResult.nextRoomId == nextRoomId,
			"PeerSessionService: room change next room"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(previousRoomId) == 0,
			"PeerSessionService: previous room empty"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(nextRoomId) == 1,
			"PeerSessionService: next room count"
		);

		const server::service::PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr,
			"PeerSessionService: room change peer exists"
		);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->persistentPlayerId == testPersistentPlayerId,
				"PeerSessionService: room change persistent player id preserved"
			);

			tests::Expect(
				result,
				peerState->roomId == nextRoomId,
				"PeerSessionService: peer room changed"
			);

			tests::Expect(
				result,
				peerState->lastRecvTime == changeTime,
				"PeerSessionService: room change recv time"
			);
		}

		playerState = FindPlayer(gameWorld, joinResult.playerId);

		tests::Expect(
			result,
			playerState != nullptr,
			"PeerSessionService: room change player exists"
		);

		if (playerState != nullptr)
		{
			tests::Expect(
				result,
				playerState->x == changeResult.spawnPosition.x,
				"PeerSessionService: room change player x"
			);

			tests::Expect(
				result,
				playerState->y == changeResult.spawnPosition.y,
				"PeerSessionService: room change player y"
			);

			tests::Expect(
				result,
				playerState->inputFlags == common::game::InputFlags::None,
				"PeerSessionService: room change clears input"
			);
		}
	}

	void RunChangePeerRoomInvalidRoomFailsTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(6);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		const TimePoint now = Clock::now();

		static_cast<void>(
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				1,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				now
			)
			);

		const server::service::PeerSessionService::RoomChangeResult
			changeResult =
			service.ChangePeerRoom(
				endpointKey,
				0,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				now
			);

		tests::Expect(
			result,
			!changeResult.changed,
			"PeerSessionService: invalid room change fails"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(1) == 1,
			"PeerSessionService: invalid room original room remains"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(0) == 0,
			"PeerSessionService: invalid room not created"
		);
	}

	void RunChangePeerRoomSameRoomFailsTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(7);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		constexpr common::game::RoomId roomId = 1;
		const TimePoint now = Clock::now();

		static_cast<void>(
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				roomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				now
			)
			);

		const server::service::PeerSessionService::RoomChangeResult
			changeResult =
			service.ChangePeerRoom(
				endpointKey,
				roomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				now
			);

		tests::Expect(
			result,
			!changeResult.changed,
			"PeerSessionService: same room change fails"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(roomId) == 1,
			"PeerSessionService: same room member count"
		);
	}

	void RunChangePeerRoomUnknownPeerFailsTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;

		const sockaddr_in remoteAddress = MakeRemoteAddress(8);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		const TimePoint now = Clock::now();

		const server::service::PeerSessionService::RoomChangeResult
			changeResult =
			service.ChangePeerRoom(
				endpointKey,
				2,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				now
			);

		tests::Expect(
			result,
			!changeResult.changed,
			"PeerSessionService: unknown room change fails"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 0,
			"PeerSessionService: unknown room change peer count"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 0,
			"PeerSessionService: unknown room change player count"
		);
	}

	void RunChangePeerRoomDeadPlayerFailsTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(9);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		constexpr common::game::RoomId roomId = 1;
		const TimePoint now = Clock::now();

		const server::service::PeerSessionService::JoinResult joinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				roomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				now
			);

		server::game::PlayerState* playerState =
			FindPlayer(gameWorld, joinResult.playerId);

		if (playerState != nullptr)
		{
			playerState->isDead = true;
		}

		const server::service::PeerSessionService::RoomChangeResult
			changeResult =
			service.ChangePeerRoom(
				endpointKey,
				2,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				now
			);

		tests::Expect(
			result,
			!changeResult.changed,
			"PeerSessionService: dead player room change fails"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(roomId) == 1,
			"PeerSessionService: dead player original room remains"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(2) == 0,
			"PeerSessionService: dead player next room empty"
		);
	}

	void RunJoinPeerAppliesReliableUdpConfigTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpRuleConfig{};
		reliableUdpRuleConfig.maxPendingPacketCount = 3;
		reliableUdpRuleConfig.maxResendCount = 1;
		reliableUdpRuleConfig.resendInterval =
			common::time::Milliseconds(150);

		const sockaddr_in remoteAddress = MakeRemoteAddress(10);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		constexpr common::game::RoomId initialRoomId = 1;
		const TimePoint now = Clock::now();

		const server::service::PeerSessionService::JoinResult joinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				initialRoomId,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpRuleConfig,
				now
			);

		tests::Expect(
			result,
			joinResult.shouldSendResponse,
			"PeerSessionService: reliable config join sends response"
		);

		tests::Expect(
			result,
			joinResult.shouldBroadcastPlayerJoined,
			"PeerSessionService: reliable config join broadcasts joined"
		);

		server::service::PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr,
			"PeerSessionService: reliable config peer exists"
		);

		if (peerState == nullptr)
		{
			return;
		}

		const bool firstRegisterResult =
			peerState->reliableSession.RegisterSentPacket(
				peerState->reliableSession.AllocateOutgoingSequence(),
				common::packet::PacketBuffer{ 'A' },
				now
			);

		const bool secondRegisterResult =
			peerState->reliableSession.RegisterSentPacket(
				peerState->reliableSession.AllocateOutgoingSequence(),
				common::packet::PacketBuffer{ 'B' },
				now
			);

		const bool thirdRegisterResult =
			peerState->reliableSession.RegisterSentPacket(
				peerState->reliableSession.AllocateOutgoingSequence(),
				common::packet::PacketBuffer{ 'C' },
				now
			);

		const bool fourthRegisterResult =
			peerState->reliableSession.RegisterSentPacket(
				peerState->reliableSession.AllocateOutgoingSequence(),
				common::packet::PacketBuffer{ 'D' },
				now
			);

		tests::Expect(
			result,
			firstRegisterResult,
			"PeerSessionService: reliable config first pending packet accepted"
		);

		tests::Expect(
			result,
			secondRegisterResult,
			"PeerSessionService: reliable config second pending packet accepted"
		);

		tests::Expect(
			result,
			thirdRegisterResult,
			"PeerSessionService: reliable config third pending packet accepted"
		);

		tests::Expect(
			result,
			!fourthRegisterResult,
			"PeerSessionService: reliable max pending packet count applied"
		);

		tests::Expect(
			result,
			peerState->reliableSession.GetPendingPacketCount() == 3,
			"PeerSessionService: reliable pending count after max check"
		);

		const common::net::ReliableUdpSession::ResendResult earlyResult =
			peerState->reliableSession.ExtractResendResult(
				now + common::time::Milliseconds(149)
			);

		tests::Expect(
			result,
			earlyResult.resendPacketList.empty(),
			"PeerSessionService: reliable resend interval blocks early resend"
		);

		tests::Expect(
			result,
			earlyResult.giveUpPacketList.empty(),
			"PeerSessionService: reliable resend interval blocks early give-up"
		);

		const common::net::ReliableUdpSession::ResendResult
			firstTimeoutResult =
			peerState->reliableSession.ExtractResendResult(
				now + common::time::Milliseconds(150)
			);

		tests::Expect(
			result,
			firstTimeoutResult.resendPacketList.size() == 3,
			"PeerSessionService: reliable resend interval applied"
		);

		tests::Expect(
			result,
			firstTimeoutResult.giveUpPacketList.empty(),
			"PeerSessionService: reliable first timeout no give-up"
		);

		const common::net::ReliableUdpSession::ResendResult
			secondTimeoutResult =
			peerState->reliableSession.ExtractResendResult(
				now + common::time::Milliseconds(300)
			);

		tests::Expect(
			result,
			secondTimeoutResult.resendPacketList.empty(),
			"PeerSessionService: reliable max resend no second resend"
		);

		tests::Expect(
			result,
			secondTimeoutResult.giveUpPacketList.size() == 3,
			"PeerSessionService: reliable max resend count applied"
		);

		tests::Expect(
			result,
			peerState->reliableSession.GetPendingPacketCount() == 0,
			"PeerSessionService: reliable give-up clears pending packets"
		);
	}

	void RunJoinPeerRejectsInvalidIdentityTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(12);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		const server::service::PeerSessionService::AuthenticatedIdentity
			invalidIdentity{
				.accountId = 0,
				.persistentPlayerId = testPersistentPlayerId,
				.sessionToken = testSessionToken,
				.nickname = "nickname",
		};

		const server::service::PeerSessionService::JoinResult joinResult =
			service.JoinPeer(
				remoteAddress,
				endpointKey,
				invalidIdentity,
				1,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpConfig,
				Clock::now()
			);

		tests::Expect(
			result,
			!joinResult.shouldSendResponse,
			"PeerSessionService: invalid identity no response"
		);

		tests::Expect(
			result,
			!joinResult.shouldBroadcastPlayerJoined,
			"PeerSessionService: invalid identity no broadcast"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 0,
			"PeerSessionService: invalid identity no peer"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 0,
			"PeerSessionService: invalid identity no player"
		);
	}

	void RunJoinPeerRejectsInvalidPersistentPlayerIdTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(15);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		const server::service::PeerSessionService::AuthenticatedIdentity
			invalidIdentity{
				.accountId = 1001,
				.persistentPlayerId = 0,
				.sessionToken = testSessionToken,
				.nickname = "nickname",
		};

		const server::service::PeerSessionService::JoinResult joinResult =
			service.JoinPeer(
				remoteAddress,
				endpointKey,
				invalidIdentity,
				1,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpConfig,
				Clock::now()
			);

		tests::Expect(
			result,
			!joinResult.shouldSendResponse,
			"PeerSessionService: invalid persistent player id no response"
		);

		tests::Expect(
			result,
			!joinResult.shouldBroadcastPlayerJoined,
			"PeerSessionService: invalid persistent player id no broadcast"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 0,
			"PeerSessionService: invalid persistent player id no peer"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 0,
			"PeerSessionService: invalid persistent player id no player"
		);
	}

	void RunJoinPeerRejectsInvalidSessionTokenTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(13);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		const server::service::PeerSessionService::AuthenticatedIdentity
			invalidIdentity{
				.accountId = 1001,
				.persistentPlayerId = testPersistentPlayerId,
				.sessionToken = common::net::invalidSessionToken,
				.nickname = "nickname",
		};

		const server::service::PeerSessionService::JoinResult joinResult =
			service.JoinPeer(
				remoteAddress,
				endpointKey,
				invalidIdentity,
				1,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpConfig,
				Clock::now()
			);

		tests::Expect(
			result,
			!joinResult.shouldSendResponse,
			"PeerSessionService: invalid token no response"
		);

		tests::Expect(
			result,
			!joinResult.shouldBroadcastPlayerJoined,
			"PeerSessionService: invalid token no broadcast"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 0,
			"PeerSessionService: invalid token no peer"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 0,
			"PeerSessionService: invalid token no player"
		);
	}

	void RunJoinExistingPeerRejectsDifferentPersistentPlayerIdTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(16);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		const TimePoint firstJoinTime = Clock::now();

		const server::service::PeerSessionService::JoinResult firstJoinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				1,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpConfig,
				firstJoinTime
			);

		tests::Expect(
			result,
			firstJoinResult.shouldSendResponse,
			"PeerSessionService: persistent player mismatch setup joined"
		);

		const server::service::PeerSessionService::AuthenticatedIdentity
			differentIdentity{
				.accountId = 1001,
				.persistentPlayerId = otherPersistentPlayerId,
				.sessionToken = testSessionToken,
				.nickname = "nickname",
		};

		const server::service::PeerSessionService::JoinResult retryJoinResult =
			service.JoinPeer(
				remoteAddress,
				endpointKey,
				differentIdentity,
				1,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpConfig,
				firstJoinTime + std::chrono::seconds(1)
			);

		tests::Expect(
			result,
			!retryJoinResult.shouldSendResponse,
			"PeerSessionService: different persistent player id rejected"
		);

		tests::Expect(
			result,
			!retryJoinResult.shouldBroadcastPlayerJoined,
			"PeerSessionService: persistent player mismatch no broadcast"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 1,
			"PeerSessionService: persistent player mismatch preserves peer"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 1,
			"PeerSessionService: persistent player mismatch preserves player"
		);

		const server::service::PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr
			&& peerState->persistentPlayerId == testPersistentPlayerId,
			"PeerSessionService: persistent player mismatch does not replace identity"
		);

		tests::Expect(
			result,
			peerState != nullptr
			&& peerState->lastRecvTime == firstJoinTime,
			"PeerSessionService: persistent player mismatch does not refresh receive time"
		);
	}

	void RunJoinExistingPeerRejectsDifferentSessionTokenTest(
		tests::DebugTestResult& result
	)
	{
		server::service::PeerSessionService service;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::net::ReliableUdpConfig reliableUdpConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(14);
		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(remoteAddress);

		const TimePoint firstJoinTime = Clock::now();

		const server::service::PeerSessionService::JoinResult firstJoinResult =
			JoinPeerForTest(
				service,
				remoteAddress,
				endpointKey,
				1,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpConfig,
				firstJoinTime
			);

		tests::Expect(
			result,
			firstJoinResult.shouldSendResponse,
			"PeerSessionService: token mismatch setup joined"
		);

		const server::service::PeerSessionService::AuthenticatedIdentity
			differentIdentity{
				.accountId = 1001,
				.persistentPlayerId = testPersistentPlayerId,
				.sessionToken = otherSessionToken,
				.nickname = "nickname",
		};

		const server::service::PeerSessionService::JoinResult retryJoinResult =
			service.JoinPeer(
				remoteAddress,
				endpointKey,
				differentIdentity,
				1,
				peerRoomManager,
				gameWorld,
				gameSimulation,
				gameRuleConfig,
				reliableUdpConfig,
				firstJoinTime + std::chrono::seconds(1)
			);

		tests::Expect(
			result,
			!retryJoinResult.shouldSendResponse,
			"PeerSessionService: different existing token rejected"
		);

		tests::Expect(
			result,
			!retryJoinResult.shouldBroadcastPlayerJoined,
			"PeerSessionService: different token no broadcast"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 1,
			"PeerSessionService: different token preserves existing peer"
		);

		tests::Expect(
			result,
			gameWorld.GetPlayerCount() == 1,
			"PeerSessionService: different token preserves existing player"
		);

		const server::service::PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr
			&& peerState->persistentPlayerId == testPersistentPlayerId
			&& peerState->sessionToken == testSessionToken,
			"PeerSessionService: different token does not replace session"
		);

		tests::Expect(
			result,
			peerState != nullptr
			&& peerState->lastRecvTime == firstJoinTime,
			"PeerSessionService: rejected token does not refresh receive time"
		);
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
		RunJoinPeerAppliesReliableUdpConfigTest(result);
		RunJoinExistingPeerReturnsExistingPlayerTest(result);
		RunJoinExistingPeerReturnsCurrentStateTest(result);
		RunLeavePeerRemovesPeerAndPlayerTest(result);
		RunLeaveUnknownPeerDoesNothingTest(result);
		RunChangePeerRoomTest(result);
		RunChangePeerRoomInvalidRoomFailsTest(result);
		RunChangePeerRoomSameRoomFailsTest(result);
		RunChangePeerRoomUnknownPeerFailsTest(result);
		RunChangePeerRoomDeadPlayerFailsTest(result);

		return result;
	}
}