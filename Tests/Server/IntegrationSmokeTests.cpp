#include "IntegrationSmokeTests.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <Common/Game/GameRules.h>
#include <Common/Game/InputFlags.h>
#include <Common/Game/WeaponRules.h>
#include <Common/Net/EndpointKey.h>
#include <Common/Net/SessionToken.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/PlayerSimulationContext.h>
#include <Server/Game/PlayerState.h>
#include <Server/Protocol/SnapshotBroadcastBuilder.h>
#include <Server/Protocol/SnapshotBroadcastTask.h>
#include <Server/Service/PeerRoomManager.h>
#include <Server/Service/PeerSessionService.h>
#include <Server/Service/PeerState.h>
#include <Server/Service/PlayerCommandService.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using Clock = common::time::Clock;
	using TimePoint = common::time::TimePoint;

	[[nodiscard]] constexpr common::net::EndpointKey MakeEndpointKey(std::uint32_t index) noexcept
	{
		return common::net::EndpointKey{
			.address = 0x7F000001 + index,
			.port = static_cast<std::uint16_t>(10000 + index),
		};
	}

	[[nodiscard]] bool ContainsEndpointKey(
		const server::protocol::EndpointKeyList& endpointKeyList,
		const common::net::EndpointKey& endpointKey
	) noexcept
	{
		for (const common::net::EndpointKey& currentEndpointKey : endpointKeyList)
		{
			if (currentEndpointKey == endpointKey)
			{
				return true;
			}
		}

		return false;
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

	[[nodiscard]] server::game::PlayerSimulationContextList BuildPlayerSimulationContextList(
		const server::service::PeerRoomManager& peerRoomManager
	)
	{
		server::game::PlayerSimulationContextList playerContextList;
		playerContextList.reserve(peerRoomManager.GetJoinedPeerCount());

		peerRoomManager.ForEachJoinedPeer(
			[&playerContextList](const server::service::PeerState& peerState)
			{
				playerContextList.push_back(
					server::game::PlayerSimulationContext{
						.playerId = peerState.playerId,
						.persistentPlayerId = peerState.persistentPlayerId,
						.roomId = peerState.roomId,
					}
					);
			}
		);

		return playerContextList;
	}

	[[nodiscard]] server::service::PeerSessionService::JoinResult JoinPeerForTest(
		const server::service::PeerSessionService& peerSessionService,
		const common::net::EndpointKey& endpointKey,
		std::int64_t accountId,
		std::int64_t persistentPlayerId,
		common::game::RoomId roomId,
		server::service::PeerRoomManager& peerRoomManager,
		server::game::GameWorld& gameWorld,
		const common::game::GameRuleConfig& gameRuleConfig,
		TimePoint currentTime
	)
	{
		const common::net::SessionToken sessionToken{
			.high = static_cast<std::uint64_t>(endpointKey.address),
			.low = static_cast<std::uint64_t>(endpointKey.port),
		};

		const server::service::PeerSessionService::AuthenticatedIdentity authenticatedIdentity{
			.accountId = accountId,
			.persistentPlayerId = persistentPlayerId,
			.sessionToken = sessionToken,
			.nickname = "nickname",
		};

		return peerSessionService.JoinPeer(
			endpointKey,
			authenticatedIdentity,
			roomId,
			peerRoomManager,
			gameWorld,
			gameRuleConfig,
			currentTime
		);
	}

	void RunJoinInputFireSnapshotSmokeTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService peerSessionService;
		server::service::PlayerCommandService playerCommandService;
		server::protocol::SnapshotBroadcastBuilder snapshotBroadcastBuilder;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		server::game::GameSimulation gameSimulation;
		common::game::GameRuleConfig gameRuleConfig{};
		common::game::WeaponRuleConfig weaponRuleConfig{};

		const common::net::EndpointKey firstEndpointKey = MakeEndpointKey(1);
		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(2);

		constexpr std::int64_t firstAccountId = 1001;
		constexpr std::int64_t secondAccountId = 1002;

		constexpr std::int64_t firstPersistentPlayerId = 5001;
		constexpr std::int64_t secondPersistentPlayerId = 5002;

		constexpr common::game::RoomId roomId = 1;

		const TimePoint startTime = Clock::now();

		const server::service::PeerSessionService::JoinResult firstJoinResult = JoinPeerForTest(
			peerSessionService,
			firstEndpointKey,
			firstAccountId,
			firstPersistentPlayerId,
			roomId,
			peerRoomManager,
			gameWorld,
			gameRuleConfig,
			startTime
		);

		const server::service::PeerSessionService::JoinResult secondJoinResult = JoinPeerForTest(
			peerSessionService,
			secondEndpointKey,
			secondAccountId,
			secondPersistentPlayerId,
			roomId,
			peerRoomManager,
			gameWorld,
			gameRuleConfig,
			startTime + common::time::Milliseconds(1)
		);

		tests::Expect(result, firstJoinResult.shouldSendResponse, "IntegrationSmoke: first join sends response");
		tests::Expect(result, firstJoinResult.shouldBroadcastPlayerJoined, "IntegrationSmoke: first join broadcasts");
		tests::Expect(result, secondJoinResult.shouldSendResponse, "IntegrationSmoke: second join sends response");
		tests::Expect(result, secondJoinResult.shouldBroadcastPlayerJoined, "IntegrationSmoke: second join broadcasts");

		tests::Expect(result, peerRoomManager.GetPeerCount() == 2, "IntegrationSmoke: peer count after join");
		tests::Expect(result, peerRoomManager.GetRoomMemberCount(roomId) == 2, "IntegrationSmoke: room member count after join");
		tests::Expect(result, gameWorld.GetPlayerCount() == 2, "IntegrationSmoke: player count after join");

		const server::service::PeerState* firstPeerState = peerRoomManager.FindJoinedPeer(firstEndpointKey);
		const server::service::PeerState* secondPeerState = peerRoomManager.FindJoinedPeer(secondEndpointKey);

		tests::Expect(
			result,
			firstPeerState != nullptr
			&& firstPeerState->endpointKey == firstEndpointKey
			&& firstPeerState->accountId == firstAccountId
			&& firstPeerState->persistentPlayerId == firstPersistentPlayerId,
			"IntegrationSmoke: first persistent identity stored"
		);

		tests::Expect(
			result,
			secondPeerState != nullptr
			&& secondPeerState->endpointKey == secondEndpointKey
			&& secondPeerState->accountId == secondAccountId
			&& secondPeerState->persistentPlayerId == secondPersistentPlayerId,
			"IntegrationSmoke: second persistent identity stored"
		);

		common::packet::InputCommandPacket inputCommandPacket{};
		inputCommandPacket.inputSequence = 1;
		inputCommandPacket.inputFlags = common::game::InputFlags::Right;

		const TimePoint inputTime = startTime + common::time::Milliseconds(10);

		const bool inputApplied = playerCommandService.ApplyInputCommand(
			firstEndpointKey,
			inputCommandPacket.inputSequence,
			inputCommandPacket.inputFlags,
			peerRoomManager,
			gameWorld,
			inputTime
		);

		tests::Expect(result, inputApplied, "IntegrationSmoke: input command applied");

		const TimePoint fireTime = startTime + common::time::Milliseconds(20);

		const bool fireSucceeded =
			playerCommandService.FireBullet(firstEndpointKey, peerRoomManager, gameWorld, weaponRuleConfig, fireTime);

		tests::Expect(result, fireSucceeded, "IntegrationSmoke: fire succeeds");
		tests::Expect(result, gameWorld.GetBulletCount() == 1, "IntegrationSmoke: bullet count after fire");

		const server::game::PlayerState* firstPlayerStateBeforeUpdate = gameWorld.FindPlayer(firstJoinResult.playerId);

		const float firstPlayerXBeforeUpdate =
			(firstPlayerStateBeforeUpdate != nullptr) ? firstPlayerStateBeforeUpdate->x : 0.0F;

		const server::game::PlayerSimulationContextList playerContextList =
			BuildPlayerSimulationContextList(peerRoomManager);

		gameSimulation.UpdatePlayers(0.05F, playerContextList, gameWorld);
		gameWorld.AdvanceServerTick();

		const server::game::PlayerState* firstPlayerStateAfterUpdate = gameWorld.FindPlayer(firstJoinResult.playerId);

		tests::Expect(result, firstPlayerStateAfterUpdate != nullptr, "IntegrationSmoke: first player exists after update");

		if (firstPlayerStateAfterUpdate != nullptr)
		{
			tests::Expect(
				result,
				firstPlayerStateAfterUpdate->x > firstPlayerXBeforeUpdate,
				"IntegrationSmoke: input moved first player"
			);

			tests::Expect(
				result,
				firstPlayerStateAfterUpdate->inputFlags == common::game::InputFlags::Right,
				"IntegrationSmoke: first player input flag kept"
			);

			tests::Expect(
				result,
				firstPlayerStateAfterUpdate->fireCooldownRemainingSeconds > 0.0F,
				"IntegrationSmoke: fire cooldown set"
			);
		}

		const std::vector<server::protocol::PlayerSnapshotTask> playerSnapshotTaskList =
			snapshotBroadcastBuilder.BuildPlayerSnapshotTasks(
				peerRoomManager.GetRoomTable(),
				peerRoomManager.GetPeerTable(),
				gameWorld
			);

		const std::vector<server::protocol::BulletSnapshotTask> bulletSnapshotTaskList =
			snapshotBroadcastBuilder.BuildBulletSnapshotTasks(
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

			const common::packet::PlayerStateData* firstPlayerData =
				FindPlayerStateData(packet, firstJoinResult.playerId);

			const common::packet::PlayerStateData* secondPlayerData =
				FindPlayerStateData(packet, secondJoinResult.playerId);

			tests::Expect(result, firstPlayerData != nullptr, "IntegrationSmoke: first player in snapshot");
			tests::Expect(result, secondPlayerData != nullptr, "IntegrationSmoke: second player in snapshot");

			if (playerSnapshotTask.endpointKey == firstEndpointKey)
			{
				firstPlayerSnapshotFound = true;

				tests::Expect(
					result,
					packet.lastProcessedInputSequence == 1,
					"IntegrationSmoke: first snapshot last input sequence"
				);
			}
			else if (playerSnapshotTask.endpointKey == secondEndpointKey)
			{
				secondPlayerSnapshotFound = true;

				tests::Expect(
					result,
					packet.lastProcessedInputSequence == 0,
					"IntegrationSmoke: second snapshot last input sequence"
				);
			}
			else
			{
				tests::Expect(result, false, "IntegrationSmoke: unexpected player snapshot endpoint");
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

			tests::Expect(
				result,
				bulletSnapshotTask.endpointKeyList.size() == 2,
				"IntegrationSmoke: bullet snapshot endpoint count"
			);

			tests::Expect(
				result,
				ContainsEndpointKey(bulletSnapshotTask.endpointKeyList, firstEndpointKey),
				"IntegrationSmoke: bullet snapshot first endpoint"
			);

			tests::Expect(
				result,
				ContainsEndpointKey(bulletSnapshotTask.endpointKeyList, secondEndpointKey),
				"IntegrationSmoke: bullet snapshot second endpoint"
			);

			const common::packet::BulletStateData* bulletData = FindBulletStateData(packet, 1);

			tests::Expect(result, bulletData != nullptr, "IntegrationSmoke: fired bullet in snapshot");

			if (bulletData != nullptr && firstPlayerStateBeforeUpdate != nullptr)
			{
				tests::Expect(result, bulletData->x == firstPlayerXBeforeUpdate, "IntegrationSmoke: bullet snapshot x");
				tests::Expect(result, bulletData->y == firstPlayerStateBeforeUpdate->y, "IntegrationSmoke: bullet snapshot y");
			}

			const std::optional<common::packet::PacketBuffer> serializedBulletPacket =
				common::packet::SerializePacket(packet);

			tests::Expect(
				result,
				serializedBulletPacket.has_value(),
				"IntegrationSmoke: bullet snapshot serializes"
			);
		}
	}

	void RunRoomChangeSnapshotSmokeTest(tests::DebugTestResult& result)
	{
		server::service::PeerSessionService peerSessionService;
		server::protocol::SnapshotBroadcastBuilder snapshotBroadcastBuilder;
		server::service::PeerRoomManager peerRoomManager;
		server::game::GameWorld gameWorld;
		common::game::GameRuleConfig gameRuleConfig{};

		const common::net::EndpointKey firstEndpointKey = MakeEndpointKey(10);
		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(11);

		constexpr std::int64_t firstAccountId = 1010;
		constexpr std::int64_t secondAccountId = 1011;

		constexpr std::int64_t firstPersistentPlayerId = 5010;
		constexpr std::int64_t secondPersistentPlayerId = 5011;

		constexpr common::game::RoomId firstRoomId = 1;
		constexpr common::game::RoomId secondRoomId = 2;

		const TimePoint startTime = Clock::now();

		const server::service::PeerSessionService::JoinResult firstJoinResult = JoinPeerForTest(
			peerSessionService,
			firstEndpointKey,
			firstAccountId,
			firstPersistentPlayerId,
			firstRoomId,
			peerRoomManager,
			gameWorld,
			gameRuleConfig,
			startTime
		);

		const server::service::PeerSessionService::JoinResult secondJoinResult = JoinPeerForTest(
			peerSessionService,
			secondEndpointKey,
			secondAccountId,
			secondPersistentPlayerId,
			firstRoomId,
			peerRoomManager,
			gameWorld,
			gameRuleConfig,
			startTime + common::time::Milliseconds(1)
		);

		const server::service::PeerSessionService::RoomChangeResult roomChangeResult =
			peerSessionService.ChangePeerRoom(
				firstEndpointKey,
				secondRoomId,
				peerRoomManager,
				gameWorld,
				startTime + common::time::Milliseconds(10)
			);

		gameWorld.AdvanceServerTick();

		tests::Expect(result, roomChangeResult.changed, "IntegrationSmoke: room change succeeds");
		tests::Expect(result, roomChangeResult.playerId == firstJoinResult.playerId, "IntegrationSmoke: room change player id");
		tests::Expect(
			result,
			roomChangeResult.persistentPlayerId == firstPersistentPlayerId,
			"IntegrationSmoke: room change persistent player id"
		);
		tests::Expect(result, roomChangeResult.previousRoomId == firstRoomId, "IntegrationSmoke: room change previous room");
		tests::Expect(result, roomChangeResult.nextRoomId == secondRoomId, "IntegrationSmoke: room change next room");

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(firstRoomId) == 1,
			"IntegrationSmoke: first room member count"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(secondRoomId) == 1,
			"IntegrationSmoke: second room member count"
		);

		const server::service::PeerState* firstPeerState = peerRoomManager.FindJoinedPeer(firstEndpointKey);

		tests::Expect(
			result,
			firstPeerState != nullptr
			&& firstPeerState->endpointKey == firstEndpointKey
			&& firstPeerState->accountId == firstAccountId
			&& firstPeerState->persistentPlayerId == firstPersistentPlayerId,
			"IntegrationSmoke: room change preserves persistent identity"
		);

		const std::vector<server::protocol::PlayerSnapshotTask> playerSnapshotTaskList =
			snapshotBroadcastBuilder.BuildPlayerSnapshotTasks(
				peerRoomManager.GetRoomTable(),
				peerRoomManager.GetPeerTable(),
				gameWorld
			);

		tests::Expect(
			result,
			playerSnapshotTaskList.size() == 2,
			"IntegrationSmoke: room split player snapshot task count"
		);

		for (const server::protocol::PlayerSnapshotTask& playerSnapshotTask : playerSnapshotTaskList)
		{
			const common::packet::PlayerSnapshotPacket& packet = playerSnapshotTask.snapshotPacket;

			if (packet.roomId == firstRoomId)
			{
				tests::Expect(
					result,
					playerSnapshotTask.endpointKey == secondEndpointKey,
					"IntegrationSmoke: first room snapshot endpoint"
				);

				tests::Expect(result, packet.playerCount == 1, "IntegrationSmoke: first room snapshot player count");

				tests::Expect(
					result,
					FindPlayerStateData(packet, secondJoinResult.playerId) != nullptr,
					"IntegrationSmoke: second player remains in first room snapshot"
				);

				tests::Expect(
					result,
					FindPlayerStateData(packet, firstJoinResult.playerId) == nullptr,
					"IntegrationSmoke: first player removed from first room snapshot"
				);
			}
			else if (packet.roomId == secondRoomId)
			{
				tests::Expect(
					result,
					playerSnapshotTask.endpointKey == firstEndpointKey,
					"IntegrationSmoke: second room snapshot endpoint"
				);

				tests::Expect(result, packet.playerCount == 1, "IntegrationSmoke: second room snapshot player count");

				tests::Expect(
					result,
					FindPlayerStateData(packet, firstJoinResult.playerId) != nullptr,
					"IntegrationSmoke: first player appears in second room snapshot"
				);

				tests::Expect(
					result,
					FindPlayerStateData(packet, secondJoinResult.playerId) == nullptr,
					"IntegrationSmoke: second player excluded from second room snapshot"
				);
			}
			else
			{
				tests::Expect(result, false, "IntegrationSmoke: unexpected room split snapshot");
			}

			const std::optional<common::packet::PacketBuffer> serializedPlayerPacket =
				common::packet::SerializePacket(packet);

			tests::Expect(
				result,
				serializedPlayerPacket.has_value(),
				"IntegrationSmoke: room split player snapshot serializes"
			);
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