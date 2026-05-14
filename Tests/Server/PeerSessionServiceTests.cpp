#include "PeerSessionServiceTests.h"

#include <WinSock2.h>

#include <chrono>
#include <cstdint>

#include <Common/Diagnostics/DebugTestResult.h>
#include <Common/Game/InputFlags.h>
#include <Common/Net/Endpoint.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/PlayerState.h>
#include <Server/Net/PeerRoomManager.h>
#include <Server/Net/PeerSessionService.h>
#include <Server/Net/PeerState.h>

namespace
{
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	[[nodiscard]] sockaddr_in MakeRemoteAddress(std::uint32_t index) noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_addr.S_un.S_addr = ::htonl(0x7F000001 + index);
		remoteAddress.sin_port = ::htons(static_cast<u_short>(10000 + index));
		return remoteAddress;
	}

	[[nodiscard]] common::net::EndpointKey MakeEndpointKey(const sockaddr_in& remoteAddress) noexcept
	{
		return common::net::MakeEndpointKey(remoteAddress);
	}

	[[nodiscard]] server::net::PeerSessionService::JoinResult JoinPeerForTest(
		const server::net::PeerSessionService& service,
		const sockaddr_in& remoteAddress,
		const common::net::EndpointKey& endpointKey,
		common::game::RoomId initialRoomId,
		server::net::PeerRoomManager& peerRoomManager,
		server::game::GameWorld& gameWorld,
		const server::game::GameSimulation& gameSimulation,
		const server::config::GameRuleConfig& gameRuleConfig,
		TimePoint currentTime
	)
	{
		return service.JoinPeer(
			remoteAddress,
			endpointKey,
			initialRoomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
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

	void RunJoinPeerCreatesPeerAndPlayerTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PeerSessionService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::GameRuleConfig gameRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(1);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);
		constexpr common::game::RoomId initialRoomId = 1;
		const TimePoint now = Clock::now();

		const server::net::PeerSessionService::JoinResult joinResult = JoinPeerForTest(
			service,
			remoteAddress,
			endpointKey,
			initialRoomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			now
		);

		common::diagnostics::Expect(result, joinResult.shouldSendResponse, "PeerSessionService: join sends response");
		common::diagnostics::Expect(result, joinResult.shouldBroadcastPlayerJoined, "PeerSessionService: join broadcasts joined");
		common::diagnostics::Expect(result, joinResult.playerId == 1, "PeerSessionService: join allocates first player id");
		common::diagnostics::Expect(result, joinResult.roomId == initialRoomId, "PeerSessionService: join room id");
		common::diagnostics::Expect(result, peerRoomManager.GetPeerCount() == 1, "PeerSessionService: join peer count");
		common::diagnostics::Expect(result, peerRoomManager.GetJoinedPeerCount() == 1, "PeerSessionService: join joined peer count");
		common::diagnostics::Expect(result, peerRoomManager.GetRoomMemberCount(initialRoomId) == 1, "PeerSessionService: join room count");
		common::diagnostics::Expect(result, gameWorld.GetPlayerCount() == 1, "PeerSessionService: join player count");

		const server::net::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);
		common::diagnostics::Expect(result, peerState != nullptr, "PeerSessionService: join peer exists");

		if (peerState != nullptr)
		{
			common::diagnostics::Expect(result, peerState->playerId == joinResult.playerId, "PeerSessionService: join peer player id");
			common::diagnostics::Expect(result, peerState->roomId == initialRoomId, "PeerSessionService: join peer room id");
			common::diagnostics::Expect(result, peerState->lastRecvTime == now, "PeerSessionService: join lastRecvTime");
		}

		const server::game::PlayerState* playerState = gameWorld.FindPlayer(joinResult.playerId);
		common::diagnostics::Expect(result, playerState != nullptr, "PeerSessionService: join player exists");

		if (playerState != nullptr)
		{
			common::diagnostics::Expect(result, playerState->playerId == joinResult.playerId, "PeerSessionService: join player id");
			common::diagnostics::Expect(result, playerState->x == joinResult.spawnPosition.x, "PeerSessionService: join player x");
			common::diagnostics::Expect(result, playerState->y == joinResult.spawnPosition.y, "PeerSessionService: join player y");
			common::diagnostics::Expect(result, playerState->hp == gameRuleConfig.initialPlayerHp, "PeerSessionService: join player hp");
			common::diagnostics::Expect(result, !playerState->isDead, "PeerSessionService: join player alive");
		}
	}

	void RunJoinExistingPeerReturnsExistingPlayerTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PeerSessionService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::GameRuleConfig gameRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(2);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);
		constexpr common::game::RoomId initialRoomId = 1;

		const TimePoint firstTime = Clock::now();
		const TimePoint secondTime = firstTime + std::chrono::seconds(5);

		const server::net::PeerSessionService::JoinResult firstJoinResult = JoinPeerForTest(
			service,
			remoteAddress,
			endpointKey,
			initialRoomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			firstTime
		);

		const server::net::PeerSessionService::JoinResult secondJoinResult = JoinPeerForTest(
			service,
			remoteAddress,
			endpointKey,
			initialRoomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			secondTime
		);

		common::diagnostics::Expect(result, firstJoinResult.shouldBroadcastPlayerJoined, "PeerSessionService: existing join first broadcast");
		common::diagnostics::Expect(result, secondJoinResult.shouldSendResponse, "PeerSessionService: existing join sends response");
		common::diagnostics::Expect(result, !secondJoinResult.shouldBroadcastPlayerJoined, "PeerSessionService: existing join no broadcast");
		common::diagnostics::Expect(result, secondJoinResult.playerId == firstJoinResult.playerId, "PeerSessionService: existing join same player");
		common::diagnostics::Expect(result, peerRoomManager.GetPeerCount() == 1, "PeerSessionService: existing join peer count");
		common::diagnostics::Expect(result, gameWorld.GetPlayerCount() == 1, "PeerSessionService: existing join player count");

		const server::net::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);
		common::diagnostics::Expect(result, peerState != nullptr, "PeerSessionService: existing join peer exists");

		if (peerState != nullptr)
		{
			common::diagnostics::Expect(result, peerState->lastRecvTime == secondTime, "PeerSessionService: existing join refresh time");
		}
	}

	void RunLeavePeerRemovesPeerAndPlayerTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PeerSessionService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::GameRuleConfig gameRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(3);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);
		constexpr common::game::RoomId roomId = 1;
		const TimePoint now = Clock::now();

		const server::net::PeerSessionService::JoinResult joinResult = JoinPeerForTest(
			service,
			remoteAddress,
			endpointKey,
			roomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			now
		);

		const server::net::PeerSessionService::LeaveResult leaveResult = service.LeavePeer(
			endpointKey,
			peerRoomManager,
			gameWorld
		);

		common::diagnostics::Expect(result, leaveResult.shouldBroadcastPlayerLeft, "PeerSessionService: leave broadcasts left");
		common::diagnostics::Expect(result, leaveResult.playerId == joinResult.playerId, "PeerSessionService: leave player id");
		common::diagnostics::Expect(result, leaveResult.roomId == roomId, "PeerSessionService: leave room id");
		common::diagnostics::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: leave peer count");
		common::diagnostics::Expect(result, peerRoomManager.GetRoomMemberCount(roomId) == 0, "PeerSessionService: leave room count");
		common::diagnostics::Expect(result, gameWorld.GetPlayerCount() == 0, "PeerSessionService: leave player count");
		common::diagnostics::Expect(result, gameWorld.FindPlayer(joinResult.playerId) == nullptr, "PeerSessionService: leave player removed");
	}

	void RunLeaveUnknownPeerDoesNothingTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PeerSessionService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;

		const sockaddr_in remoteAddress = MakeRemoteAddress(4);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);

		const server::net::PeerSessionService::LeaveResult leaveResult = service.LeavePeer(
			endpointKey,
			peerRoomManager,
			gameWorld
		);

		common::diagnostics::Expect(result, !leaveResult.shouldBroadcastPlayerLeft, "PeerSessionService: unknown leave no broadcast");
		common::diagnostics::Expect(result, leaveResult.playerId == 0, "PeerSessionService: unknown leave player id");
		common::diagnostics::Expect(result, leaveResult.roomId == 0, "PeerSessionService: unknown leave room id");
		common::diagnostics::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: unknown leave peer count");
		common::diagnostics::Expect(result, gameWorld.GetPlayerCount() == 0, "PeerSessionService: unknown leave player count");
	}

	void RunChangePeerRoomTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PeerSessionService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::GameRuleConfig gameRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(5);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);
		constexpr common::game::RoomId previousRoomId = 1;
		constexpr common::game::RoomId nextRoomId = 2;
		const TimePoint joinTime = Clock::now();
		const TimePoint changeTime = joinTime + std::chrono::seconds(1);

		const server::net::PeerSessionService::JoinResult joinResult = JoinPeerForTest(
			service,
			remoteAddress,
			endpointKey,
			previousRoomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			joinTime
		);

		server::game::PlayerState* playerState = FindPlayer(gameWorld, joinResult.playerId);
		if (playerState != nullptr)
		{
			playerState->inputFlags = common::game::InputFlags::Up;
		}

		const server::net::PeerSessionService::RoomChangeResult changeResult = service.ChangePeerRoom(
			endpointKey,
			nextRoomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			changeTime
		);

		common::diagnostics::Expect(result, changeResult.changed, "PeerSessionService: room change succeeds");
		common::diagnostics::Expect(result, changeResult.playerId == joinResult.playerId, "PeerSessionService: room change player id");
		common::diagnostics::Expect(result, changeResult.previousRoomId == previousRoomId, "PeerSessionService: room change previous room");
		common::diagnostics::Expect(result, changeResult.nextRoomId == nextRoomId, "PeerSessionService: room change next room");
		common::diagnostics::Expect(result, peerRoomManager.GetRoomMemberCount(previousRoomId) == 0, "PeerSessionService: previous room empty");
		common::diagnostics::Expect(result, peerRoomManager.GetRoomMemberCount(nextRoomId) == 1, "PeerSessionService: next room count");

		const server::net::PeerState* peerState = peerRoomManager.FindJoinedPeer(endpointKey);
		common::diagnostics::Expect(result, peerState != nullptr, "PeerSessionService: room change peer exists");

		if (peerState != nullptr)
		{
			common::diagnostics::Expect(result, peerState->roomId == nextRoomId, "PeerSessionService: peer room changed");
			common::diagnostics::Expect(result, peerState->lastRecvTime == changeTime, "PeerSessionService: room change recv time");
		}

		playerState = FindPlayer(gameWorld, joinResult.playerId);
		common::diagnostics::Expect(result, playerState != nullptr, "PeerSessionService: room change player exists");

		if (playerState != nullptr)
		{
			common::diagnostics::Expect(result, playerState->x == changeResult.spawnPosition.x, "PeerSessionService: room change player x");
			common::diagnostics::Expect(result, playerState->y == changeResult.spawnPosition.y, "PeerSessionService: room change player y");
			common::diagnostics::Expect(result, playerState->inputFlags == common::game::InputFlags::None,
				"PeerSessionService: room change clears input");
		}
	}

	void RunChangePeerRoomInvalidRoomFailsTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PeerSessionService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::GameRuleConfig gameRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(6);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);
		const TimePoint now = Clock::now();

		JoinPeerForTest(
			service,
			remoteAddress,
			endpointKey,
			1,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			now
		);

		const server::net::PeerSessionService::RoomChangeResult changeResult = service.ChangePeerRoom(
			endpointKey,
			0,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			now
		);

		common::diagnostics::Expect(result, !changeResult.changed, "PeerSessionService: invalid room change fails");
		common::diagnostics::Expect(result, peerRoomManager.GetRoomMemberCount(1) == 1, "PeerSessionService: invalid room original room remains");
		common::diagnostics::Expect(result, peerRoomManager.GetRoomMemberCount(0) == 0, "PeerSessionService: invalid room not created");
	}

	void RunChangePeerRoomSameRoomFailsTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PeerSessionService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::GameRuleConfig gameRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(7);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);
		constexpr common::game::RoomId roomId = 1;
		const TimePoint now = Clock::now();

		JoinPeerForTest(
			service,
			remoteAddress,
			endpointKey,
			roomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			now
		);

		const server::net::PeerSessionService::RoomChangeResult changeResult = service.ChangePeerRoom(
			endpointKey,
			roomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			now
		);

		common::diagnostics::Expect(result, !changeResult.changed, "PeerSessionService: same room change fails");
		common::diagnostics::Expect(result, peerRoomManager.GetRoomMemberCount(roomId) == 1, "PeerSessionService: same room member count");
	}

	void RunChangePeerRoomUnknownPeerFailsTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PeerSessionService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;

		const sockaddr_in remoteAddress = MakeRemoteAddress(8);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);
		const TimePoint now = Clock::now();

		const server::net::PeerSessionService::RoomChangeResult changeResult = service.ChangePeerRoom(
			endpointKey,
			2,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			now
		);

		common::diagnostics::Expect(result, !changeResult.changed, "PeerSessionService: unknown room change fails");
		common::diagnostics::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerSessionService: unknown room change peer count");
		common::diagnostics::Expect(result, gameWorld.GetPlayerCount() == 0, "PeerSessionService: unknown room change player count");
	}

	void RunChangePeerRoomDeadPlayerFailsTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::PeerSessionService service;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::GameRuleConfig gameRuleConfig{};

		const sockaddr_in remoteAddress = MakeRemoteAddress(9);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);
		constexpr common::game::RoomId roomId = 1;
		const TimePoint now = Clock::now();

		const server::net::PeerSessionService::JoinResult joinResult = JoinPeerForTest(
			service,
			remoteAddress,
			endpointKey,
			roomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			now
		);

		server::game::PlayerState* playerState = FindPlayer(gameWorld, joinResult.playerId);
		if (playerState != nullptr)
		{
			playerState->isDead = true;
		}

		const server::net::PeerSessionService::RoomChangeResult changeResult = service.ChangePeerRoom(
			endpointKey,
			2,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			now
		);

		common::diagnostics::Expect(result, !changeResult.changed, "PeerSessionService: dead player room change fails");
		common::diagnostics::Expect(result, peerRoomManager.GetRoomMemberCount(roomId) == 1,
			"PeerSessionService: dead player original room remains");
		common::diagnostics::Expect(result, peerRoomManager.GetRoomMemberCount(2) == 0, "PeerSessionService: dead player next room empty");
	}
}

namespace tests::server
{
	common::diagnostics::DebugTestResult RunPeerSessionServiceTests()
	{
		common::diagnostics::DebugTestResult result{};

		RunJoinPeerCreatesPeerAndPlayerTest(result);
		RunJoinExistingPeerReturnsExistingPlayerTest(result);
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