#include "IntegrationSmokeTests.h"

#include <WinSock2.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <Common/Game/InputFlags.h>
#include <Common/Net/Endpoint.h>
#include <Common/Net/SessionToken.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/PlayerState.h>
#include <Server/Net/PeerRoomManager.h>
#include <Server/Net/PeerSessionService.h>
#include <Server/Net/PlayerCommandService.h>
#include <Server/Protocol/SnapshotBroadcastBuilder.h>
#include <Server/Protocol/SnapshotBroadcastTask.h>

#include <Tests/DebugTestResult.h>

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

	[[nodiscard]] bool IsSameRemoteAddress(const sockaddr_in& lhs, const sockaddr_in& rhs) noexcept
	{
		return lhs.sin_family == rhs.sin_family
			&& lhs.sin_addr.S_un.S_addr == rhs.sin_addr.S_un.S_addr
			&& lhs.sin_port == rhs.sin_port;
	}

	[[nodiscard]] const common::packet::PlayerStateData* FindPlayerStateData(
		const common::packet::PlayerSnapshotPacket& packet,
		common::game::PlayerId playerId
	) noexcept
	{
		for (std::size_t index = 0; index < packet.playerCount; ++index)
		{
			if (packet.players[index].playerId == playerId)
			{
				return &packet.players[index];
			}
		}

		return nullptr;
	}

	[[nodiscard]] const common::packet::BulletStateData* FindBulletStateData(
		const common::packet::BulletSnapshotPacket& packet,
		common::game::BulletId bulletId
	) noexcept
	{
		for (std::size_t index = 0; index < packet.bulletCount; ++index)
		{
			if (packet.bullets[index].bulletId == bulletId)
			{
				return &packet.bullets[index];
			}
		}

		return nullptr;
	}

	[[nodiscard]] bool ContainsRemoteAddress(const server::protocol::RemoteAddressList& remoteAddressList, const sockaddr_in& remoteAddress)
	{
		for (const sockaddr_in& currentRemoteAddress : remoteAddressList)
		{
			if (IsSameRemoteAddress(currentRemoteAddress, remoteAddress))
			{
				return true;
			}
		}

		return false;
	}

	[[nodiscard]] server::net::PeerSessionService::JoinResult JoinPeerForTest(
		const server::net::PeerSessionService& peerSessionService,
		const sockaddr_in& remoteAddress,
		std::int64_t accountId,
		std::int64_t persistentPlayerId,
		common::game::RoomId roomId,
		server::net::PeerRoomManager& peerRoomManager,
		server::game::GameWorld& gameWorld,
		const server::game::GameSimulation& gameSimulation,
		const server::config::GameRuleConfig& gameRuleConfig,
		const server::config::ReliableUdpConfig& reliableUdpRuleConfig,
		TimePoint currentTime
	)
	{
		const common::net::EndpointKey endpointKey = common::net::MakeEndpointKey(remoteAddress);

		const common::net::SessionToken sessionToken{
			.high = static_cast<std::uint64_t>(remoteAddress.sin_addr.S_un.S_addr),
			.low = static_cast<std::uint64_t>(remoteAddress.sin_port),
		};

		const server::net::PeerSessionService::AuthenticatedIdentity authenticatedIdentity{
			.accountId = accountId,
			.persistentPlayerId = persistentPlayerId,
			.sessionToken = sessionToken,
			.nickname = "nickname",
		};

		return peerSessionService.JoinPeer(
			remoteAddress,
			endpointKey,
			authenticatedIdentity,
			roomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			reliableUdpRuleConfig,
			currentTime
		);
	}

	void RunJoinInputFireSnapshotSmokeTest(tests::DebugTestResult& result)
	{
		server::net::PeerSessionService peerSessionService;
		server::net::PlayerCommandService playerCommandService;
		server::protocol::SnapshotBroadcastBuilder snapshotBroadcastBuilder;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::GameRuleConfig gameRuleConfig{};
		server::config::WeaponRuleConfig weaponRuleConfig{};
		server::config::ReliableUdpConfig reliableRuleConfig{};

		const sockaddr_in firstRemoteAddress = MakeRemoteAddress(1);
		const sockaddr_in secondRemoteAddress = MakeRemoteAddress(2);
		const common::net::EndpointKey firstEndpointKey = common::net::MakeEndpointKey(firstRemoteAddress);
		const common::net::EndpointKey secondEndpointKey = common::net::MakeEndpointKey(secondRemoteAddress);

		constexpr std::int64_t firstAccountId = 1001;
		constexpr std::int64_t secondAccountId = 1002;
		constexpr std::int64_t firstPersistentPlayerId = 5001;
		constexpr std::int64_t secondPersistentPlayerId = 5002;
		constexpr common::game::RoomId roomId = 1;

		const TimePoint startTime = Clock::now();

		const server::net::PeerSessionService::JoinResult firstJoinResult = JoinPeerForTest(
			peerSessionService,
			firstRemoteAddress,
			firstAccountId,
			firstPersistentPlayerId,
			roomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			reliableRuleConfig,
			startTime
		);

		const server::net::PeerSessionService::JoinResult secondJoinResult = JoinPeerForTest(
			peerSessionService,
			secondRemoteAddress,
			secondAccountId,
			secondPersistentPlayerId,
			roomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			reliableRuleConfig,
			startTime + common::time::Milliseconds(1)
		);

		tests::Expect(result, firstJoinResult.shouldSendResponse, "IntegrationSmoke: first join sends response");
		tests::Expect(result, firstJoinResult.shouldBroadcastPlayerJoined, "IntegrationSmoke: first join broadcasts");
		tests::Expect(result, secondJoinResult.shouldSendResponse, "IntegrationSmoke: second join sends response");
		tests::Expect(result, secondJoinResult.shouldBroadcastPlayerJoined, "IntegrationSmoke: second join broadcasts");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 2, "IntegrationSmoke: peer count after join");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(roomId) == 2, "IntegrationSmoke: room member count after join");
		tests::Expect(result, gameWorld.GetPlayerCount() == 2, "IntegrationSmoke: player count after join");

		const server::net::PeerState* firstPeerState = peerRoomManager.FindJoinedPeer(firstEndpointKey);
		const server::net::PeerState* secondPeerState = peerRoomManager.FindJoinedPeer(secondEndpointKey);

		tests::Expect(
			result,
			firstPeerState != nullptr
			&& firstPeerState->accountId == firstAccountId
			&& firstPeerState->persistentPlayerId == firstPersistentPlayerId,
			"IntegrationSmoke: first persistent identity stored"
		);

		tests::Expect(
			result,
			secondPeerState != nullptr
			&& secondPeerState->accountId == secondAccountId
			&& secondPeerState->persistentPlayerId == secondPersistentPlayerId,
			"IntegrationSmoke: second persistent identity stored"
		);

		common::packet::InputCommandPacket inputCommandPacket{};
		inputCommandPacket.inputSequence = 1;
		inputCommandPacket.inputFlags = common::game::InputFlags::Right;

		const TimePoint inputTime = startTime + common::time::Milliseconds(10);

		const bool inputApplied
			= playerCommandService.ApplyInputCommand(firstEndpointKey, inputCommandPacket, peerRoomManager, gameWorld, inputTime);

		tests::Expect(result, inputApplied, "IntegrationSmoke: input command applied");

		const TimePoint fireTime = startTime + common::time::Milliseconds(20);

		const bool fireSucceeded
			= playerCommandService.FireBullet(firstEndpointKey, peerRoomManager, gameWorld, gameSimulation, weaponRuleConfig, fireTime);

		tests::Expect(result, fireSucceeded, "IntegrationSmoke: fire succeeds");
		tests::Expect(result, gameWorld.GetBulletCount() == 1, "IntegrationSmoke: bullet count after fire");

		const server::game::PlayerState* firstPlayerStateBeforeUpdate = gameWorld.FindPlayer(firstJoinResult.playerId);
		const float firstPlayerXBeforeUpdate = firstPlayerStateBeforeUpdate != nullptr ? firstPlayerStateBeforeUpdate->x : 0.0F;

		gameSimulation.UpdatePlayers(0.05F, peerRoomManager.GetPeerTable(), gameWorld);
		gameWorld.AdvanceServerTick();

		const server::game::PlayerState* firstPlayerStateAfterUpdate = gameWorld.FindPlayer(firstJoinResult.playerId);
		tests::Expect(result, firstPlayerStateAfterUpdate != nullptr, "IntegrationSmoke: first player exists after update");

		if (firstPlayerStateAfterUpdate != nullptr)
		{
			tests::Expect(result, firstPlayerStateAfterUpdate->x > firstPlayerXBeforeUpdate, "IntegrationSmoke: input moved first player");
			tests::Expect(result, firstPlayerStateAfterUpdate->inputFlags == common::game::InputFlags::Right,
				"IntegrationSmoke: first player input flag kept");
			tests::Expect(result, firstPlayerStateAfterUpdate->fireCooldownRemainingSeconds > 0.0F,
				"IntegrationSmoke: fire cooldown set");
		}

		const std::vector<server::protocol::PlayerSnapshotTask> playerSnapshotTaskList = snapshotBroadcastBuilder.BuildPlayerSnapshotTasks(
			peerRoomManager.GetRoomTable(),
			peerRoomManager.GetPeerTable(),
			gameWorld
		);

		const std::vector<server::protocol::BulletSnapshotTask> bulletSnapshotTaskList = snapshotBroadcastBuilder.BuildBulletSnapshotTasks(
			peerRoomManager.GetRoomTable(),
			peerRoomManager.GetPeerTable(),
			gameWorld
		);

		tests::Expect(result, playerSnapshotTaskList.size() == 2, "IntegrationSmoke: player snapshot task count");
		tests::Expect(result, bulletSnapshotTaskList.size() == 1, "IntegrationSmoke: bullet snapshot task count");

		bool firstPlayerSnapshotFound = false;
		bool secondPlayerSnapshotFound = false;

		for (const server::protocol::PlayerSnapshotTask& playerSnapshotTask : playerSnapshotTaskList)
		{
			const common::packet::PlayerSnapshotPacket& packet = playerSnapshotTask.snapshotPacket;

			tests::Expect(result, packet.serverTick == gameWorld.GetServerTick(), "IntegrationSmoke: player snapshot tick");
			tests::Expect(result, packet.roomId == roomId, "IntegrationSmoke: player snapshot room");
			tests::Expect(result, packet.playerCount == 2, "IntegrationSmoke: player snapshot player count");

			const common::packet::PlayerStateData* firstPlayerData = FindPlayerStateData(packet, firstJoinResult.playerId);
			const common::packet::PlayerStateData* secondPlayerData = FindPlayerStateData(packet, secondJoinResult.playerId);

			tests::Expect(result, firstPlayerData != nullptr, "IntegrationSmoke: first player in snapshot");
			tests::Expect(result, secondPlayerData != nullptr, "IntegrationSmoke: second player in snapshot");

			if (IsSameRemoteAddress(playerSnapshotTask.remoteAddress, firstRemoteAddress))
			{
				firstPlayerSnapshotFound = true;
				tests::Expect(result, packet.lastProcessedInputSequence == 1, "IntegrationSmoke: first snapshot last input sequence");
			}
			else if (IsSameRemoteAddress(playerSnapshotTask.remoteAddress, secondRemoteAddress))
			{
				secondPlayerSnapshotFound = true;
				tests::Expect(result, packet.lastProcessedInputSequence == 0, "IntegrationSmoke: second snapshot last input sequence");
			}
			else
			{
				tests::Expect(result, false, "IntegrationSmoke: unexpected player snapshot remote");
			}
		}

		tests::Expect(result, firstPlayerSnapshotFound, "IntegrationSmoke: first player snapshot target exists");
		tests::Expect(result, secondPlayerSnapshotFound, "IntegrationSmoke: second player snapshot target exists");

		if (!bulletSnapshotTaskList.empty())
		{
			const server::protocol::BulletSnapshotTask& bulletSnapshotTask = bulletSnapshotTaskList.front();
			const common::packet::BulletSnapshotPacket& packet = bulletSnapshotTask.snapshotPacket;

			tests::Expect(result, packet.serverTick == gameWorld.GetServerTick(), "IntegrationSmoke: bullet snapshot tick");
			tests::Expect(result, packet.roomId == roomId, "IntegrationSmoke: bullet snapshot room");
			tests::Expect(result, packet.chunkIndex == 0, "IntegrationSmoke: bullet snapshot chunk index");
			tests::Expect(result, packet.chunkCount == 1, "IntegrationSmoke: bullet snapshot chunk count");
			tests::Expect(result, packet.bulletCount == 1, "IntegrationSmoke: bullet snapshot bullet count");
			tests::Expect(result, bulletSnapshotTask.remoteAddressList.size() == 2, "IntegrationSmoke: bullet snapshot remote count");
			tests::Expect(result, ContainsRemoteAddress(bulletSnapshotTask.remoteAddressList, firstRemoteAddress),
				"IntegrationSmoke: bullet snapshot first remote");
			tests::Expect(result, ContainsRemoteAddress(bulletSnapshotTask.remoteAddressList, secondRemoteAddress),
				"IntegrationSmoke: bullet snapshot second remote");

			const common::packet::BulletStateData* bulletData = FindBulletStateData(packet, 1);
			tests::Expect(result, bulletData != nullptr, "IntegrationSmoke: fired bullet in snapshot");

			if (bulletData != nullptr && firstPlayerStateBeforeUpdate != nullptr)
			{
				tests::Expect(result, bulletData->x == firstPlayerXBeforeUpdate, "IntegrationSmoke: bullet snapshot x");
				tests::Expect(result, bulletData->y == firstPlayerStateBeforeUpdate->y, "IntegrationSmoke: bullet snapshot y");
			}

			const std::optional<common::packet::PacketBuffer> serializedBulletPacket = common::packet::SerializePacket(packet);
			tests::Expect(result, serializedBulletPacket.has_value(), "IntegrationSmoke: bullet snapshot serializes");
		}
	}

	void RunRoomChangeSnapshotSmokeTest(tests::DebugTestResult& result)
	{
		server::net::PeerSessionService peerSessionService;
		server::protocol::SnapshotBroadcastBuilder snapshotBroadcastBuilder;
		server::net::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		server::config::GameRuleConfig gameRuleConfig{};
		server::config::ReliableUdpConfig reliableRuleConfig{};

		const sockaddr_in firstRemoteAddress = MakeRemoteAddress(10);
		const sockaddr_in secondRemoteAddress = MakeRemoteAddress(11);
		const common::net::EndpointKey firstEndpointKey = common::net::MakeEndpointKey(firstRemoteAddress);

		constexpr std::int64_t firstAccountId = 1010;
		constexpr std::int64_t secondAccountId = 1011;
		constexpr std::int64_t firstPersistentPlayerId = 5010;
		constexpr std::int64_t secondPersistentPlayerId = 5011;
		constexpr common::game::RoomId firstRoomId = 1;
		constexpr common::game::RoomId secondRoomId = 2;

		const TimePoint startTime = Clock::now();

		const server::net::PeerSessionService::JoinResult firstJoinResult = JoinPeerForTest(
			peerSessionService,
			firstRemoteAddress,
			firstAccountId,
			firstPersistentPlayerId,
			firstRoomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			reliableRuleConfig,
			startTime
		);

		const server::net::PeerSessionService::JoinResult secondJoinResult = JoinPeerForTest(
			peerSessionService,
			secondRemoteAddress,
			secondAccountId,
			secondPersistentPlayerId,
			firstRoomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			gameRuleConfig,
			reliableRuleConfig,
			startTime + common::time::Milliseconds(1)
		);

		const server::net::PeerSessionService::RoomChangeResult roomChangeResult = peerSessionService.ChangePeerRoom(
			firstEndpointKey,
			secondRoomId,
			peerRoomManager,
			gameWorld,
			gameSimulation,
			startTime + common::time::Milliseconds(10)
		);

		gameWorld.AdvanceServerTick();

		tests::Expect(result, roomChangeResult.changed, "IntegrationSmoke: room change succeeds");
		tests::Expect(result, roomChangeResult.playerId == firstJoinResult.playerId, "IntegrationSmoke: room change player id");
		tests::Expect(result, roomChangeResult.previousRoomId == firstRoomId, "IntegrationSmoke: room change previous room");
		tests::Expect(result, roomChangeResult.nextRoomId == secondRoomId, "IntegrationSmoke: room change next room");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(firstRoomId) == 1, "IntegrationSmoke: first room member count");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(secondRoomId) == 1, "IntegrationSmoke: second room member count");

		const server::net::PeerState* firstPeerState = peerRoomManager.FindJoinedPeer(firstEndpointKey);

		tests::Expect(
			result,
			firstPeerState != nullptr
			&& firstPeerState->accountId == firstAccountId
			&& firstPeerState->persistentPlayerId == firstPersistentPlayerId,
			"IntegrationSmoke: room change preserves persistent identity"
		);

		const std::vector<server::protocol::PlayerSnapshotTask> playerSnapshotTaskList = snapshotBroadcastBuilder.BuildPlayerSnapshotTasks(
			peerRoomManager.GetRoomTable(),
			peerRoomManager.GetPeerTable(),
			gameWorld
		);

		tests::Expect(result, playerSnapshotTaskList.size() == 2, "IntegrationSmoke: room split player snapshot task count");

		for (const server::protocol::PlayerSnapshotTask& playerSnapshotTask : playerSnapshotTaskList)
		{
			const common::packet::PlayerSnapshotPacket& packet = playerSnapshotTask.snapshotPacket;

			if (packet.roomId == firstRoomId)
			{
				tests::Expect(result, packet.playerCount == 1, "IntegrationSmoke: first room snapshot player count");
				tests::Expect(result, FindPlayerStateData(packet, secondJoinResult.playerId) != nullptr,
					"IntegrationSmoke: second player remains in first room snapshot");
				tests::Expect(result, FindPlayerStateData(packet, firstJoinResult.playerId) == nullptr,
					"IntegrationSmoke: first player removed from first room snapshot");
			}
			else if (packet.roomId == secondRoomId)
			{
				tests::Expect(result, packet.playerCount == 1, "IntegrationSmoke: second room snapshot player count");
				tests::Expect(result, FindPlayerStateData(packet, firstJoinResult.playerId) != nullptr,
					"IntegrationSmoke: first player appears in second room snapshot");
				tests::Expect(result, FindPlayerStateData(packet, secondJoinResult.playerId) == nullptr,
					"IntegrationSmoke: second player excluded from second room snapshot");
			}
			else
			{
				tests::Expect(result, false, "IntegrationSmoke: unexpected room split snapshot");
			}

			const std::optional<common::packet::PacketBuffer> serializedPlayerPacket = common::packet::SerializePacket(packet);
			tests::Expect(result, serializedPlayerPacket.has_value(), "IntegrationSmoke: room split player snapshot serializes");
		}
	}
}

namespace tests::server
{
	DebugTestResult RunIntegrationSmokeTests()
	{
		DebugTestResult result{};

		RunJoinInputFireSnapshotSmokeTest(result);
		RunRoomChangeSnapshotSmokeTest(result);

		return result;
	}
}