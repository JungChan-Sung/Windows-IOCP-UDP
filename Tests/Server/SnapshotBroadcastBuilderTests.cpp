#include "SnapshotBroadcastBuilderTests.h"

#include <WinSock2.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketConstants.h>

#include <Server/Game/BulletState.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/ImpactEffectState.h>
#include <Server/Game/PlayerState.h>
#include <Server/Protocol/SnapshotBroadcastBuilder.h>
#include <Server/Protocol/SnapshotBroadcastTask.h>
#include <Server/Service/PeerState.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using EndpointKey = common::net::EndpointKey;
	using EndpointKeyList = std::vector<EndpointKey>;

	using PeerTable =
		server::protocol::SnapshotBroadcastBuilder::PeerTable;

	using RoomTable =
		server::protocol::SnapshotBroadcastBuilder::RoomTable;

	using RoomMemberSet =
		server::protocol::SnapshotBroadcastBuilder::RoomMemberSet;

	[[nodiscard]] EndpointKey MakeEndpointKey(
		std::uint32_t index
	) noexcept
	{
		EndpointKey endpointKey{};
		endpointKey.address = 0x7F000001 + index;
		endpointKey.port = static_cast<std::uint16_t>(10000 + index);
		return endpointKey;
	}

	[[nodiscard]] sockaddr_in MakeRemoteAddress(
		const EndpointKey& endpointKey
	) noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_addr.S_un.S_addr = endpointKey.address;
		remoteAddress.sin_port = endpointKey.port;
		return remoteAddress;
	}

	[[nodiscard]] bool IsSameRemoteAddress(
		const sockaddr_in& lhs,
		const sockaddr_in& rhs
	) noexcept
	{
		return lhs.sin_family == rhs.sin_family
			&& lhs.sin_addr.S_un.S_addr == rhs.sin_addr.S_un.S_addr
			&& lhs.sin_port == rhs.sin_port;
	}

	[[nodiscard]] bool ContainsRemoteAddress(
		const server::protocol::RemoteAddressList& remoteAddressList,
		const sockaddr_in& remoteAddress
	)
	{
		const auto remoteAddressIterator = std::ranges::find_if(
			remoteAddressList,
			[&remoteAddress](const sockaddr_in& currentRemoteAddress)
			{
				return IsSameRemoteAddress(
					currentRemoteAddress,
					remoteAddress
				);
			}
		);

		return remoteAddressIterator != remoteAddressList.end();
	}

	[[nodiscard]]
	const common::packet::PlayerStateData* FindPlayerStateData(
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

	[[nodiscard]]
	const common::packet::BulletStateData* FindBulletStateData(
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

	void AddRoomMember(
		RoomTable& roomTable,
		common::game::RoomId roomId,
		const EndpointKey& endpointKey
	)
	{
		roomTable[roomId].insert(endpointKey);
	}

	void AddPeer(
		PeerTable& peerTable,
		const EndpointKey& endpointKey,
		common::game::PlayerId playerId,
		common::game::RoomId roomId,
		bool isJoined,
		std::uint32_t lastInputSequence
	)
	{
		auto [peerIterator, _] =
			peerTable.try_emplace(endpointKey);

		server::service::PeerState& peerState =
			peerIterator->second;

		peerState.remoteAddress = MakeRemoteAddress(endpointKey);
		peerState.endpointKey = endpointKey;
		peerState.playerId = playerId;
		peerState.roomId = roomId;
		peerState.isJoined = isJoined;
		peerState.lastInputSequence = lastInputSequence;
	}

	void AddPlayer(
		server::game::GameWorld& gameWorld,
		common::game::PlayerId playerId,
		float x,
		float y,
		int hp,
		bool isDead
	)
	{
		server::game::PlayerState playerState{};
		playerState.playerId = playerId;
		playerState.x = x;
		playerState.y = y;
		playerState.hp = hp;
		playerState.isDead = isDead;
		playerState.killCount = playerId * 10;
		playerState.deathCount = playerId;
		playerState.respawnRemainingSeconds =
			isDead ? 2.0F : 0.0F;
		playerState.invincibilityRemainingSeconds = 1.0F;
		playerState.hitFlashRemainingSeconds = 0.25F;

		gameWorld.UpsertPlayer(playerState);
	}

	void AddBullet(
		server::game::GameWorld& gameWorld,
		common::game::BulletId bulletId,
		common::game::RoomId roomId,
		float x,
		float y
	)
	{
		server::game::BulletState bulletState{};
		bulletState.bulletId = bulletId;
		bulletState.roomId = roomId;
		bulletState.x = x;
		bulletState.y = y;

		gameWorld.AddBullet(bulletState);
	}

	void AddImpactEffect(
		server::game::GameWorld& gameWorld,
		common::game::RoomId roomId,
		common::game::EffectType effectType,
		float x,
		float y
	)
	{
		server::game::ImpactEffectState impactEffectState{};
		impactEffectState.roomId = roomId;
		impactEffectState.effectType = effectType;
		impactEffectState.x = x;
		impactEffectState.y = y;

		gameWorld.AddPendingImpactEffect(impactEffectState);
	}

	void AdvanceServerTick(
		server::game::GameWorld& gameWorld,
		std::uint32_t tickCount
	)
	{
		for (std::uint32_t tick = 0; tick < tickCount; ++tick)
		{
			gameWorld.AdvanceServerTick();
		}
	}

	void RunBuildPlayerSnapshotTasksTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		AdvanceServerTick(gameWorld, 10);

		const EndpointKey firstEndpointKey = MakeEndpointKey(1);
		const EndpointKey secondEndpointKey = MakeEndpointKey(2);
		const EndpointKey unjoinedEndpointKey = MakeEndpointKey(3);
		const EndpointKey missingPeerEndpointKey = MakeEndpointKey(99);

		AddRoomMember(roomTable, 1, firstEndpointKey);
		AddRoomMember(roomTable, 1, secondEndpointKey);
		AddRoomMember(roomTable, 1, unjoinedEndpointKey);
		AddRoomMember(roomTable, 1, missingPeerEndpointKey);

		AddPeer(peerTable, firstEndpointKey, 101, 1, true, 1001);
		AddPeer(peerTable, secondEndpointKey, 102, 1, true, 1002);
		AddPeer(peerTable, unjoinedEndpointKey, 103, 1, false, 1003);

		AddPlayer(gameWorld, 101, 10.0F, 20.0F, 3, false);
		AddPlayer(gameWorld, 102, 30.0F, 40.0F, 0, true);
		AddPlayer(gameWorld, 103, 50.0F, 60.0F, 3, false);

		const std::vector<server::protocol::PlayerSnapshotTask> taskList =
			builder.BuildPlayerSnapshotTasks(
				roomTable,
				peerTable,
				gameWorld
			);

		tests::Expect(
			result,
			taskList.size() == 2,
			"SnapshotBroadcastBuilder: player task count"
		);

		for (const server::protocol::PlayerSnapshotTask& task : taskList)
		{
			const common::packet::PlayerSnapshotPacket& packet =
				task.snapshotPacket;

			tests::Expect(result, packet.serverTick == 10, "SnapshotBroadcastBuilder: player packet serverTick");
			tests::Expect(result, packet.roomId == 1, "SnapshotBroadcastBuilder: player packet roomId");
			tests::Expect(result, packet.playerCount == 2, "SnapshotBroadcastBuilder: player packet playerCount");

			const common::packet::PlayerStateData* firstPlayerData =
				FindPlayerStateData(packet, 101);

			const common::packet::PlayerStateData* secondPlayerData =
				FindPlayerStateData(packet, 102);

			const common::packet::PlayerStateData* unjoinedPlayerData =
				FindPlayerStateData(packet, 103);

			tests::Expect(result, firstPlayerData != nullptr, "SnapshotBroadcastBuilder: first player included");
			tests::Expect(result, secondPlayerData != nullptr, "SnapshotBroadcastBuilder: second player included");
			tests::Expect(result, unjoinedPlayerData == nullptr, "SnapshotBroadcastBuilder: unjoined player excluded");

			if (firstPlayerData != nullptr)
			{
				tests::Expect(result, firstPlayerData->x == 10.0F, "SnapshotBroadcastBuilder: first player x");
				tests::Expect(result, firstPlayerData->y == 20.0F, "SnapshotBroadcastBuilder: first player y");
				tests::Expect(result, firstPlayerData->hp == 3, "SnapshotBroadcastBuilder: first player hp");
				tests::Expect(result, firstPlayerData->isDead == 0, "SnapshotBroadcastBuilder: first player alive flag");
			}

			if (secondPlayerData != nullptr)
			{
				tests::Expect(result, secondPlayerData->hp == 0, "SnapshotBroadcastBuilder: second player hp");
				tests::Expect(result, secondPlayerData->isDead == 1, "SnapshotBroadcastBuilder: second player dead flag");
			}

			if (IsSameRemoteAddress(
				task.remoteAddress,
				MakeRemoteAddress(firstEndpointKey)
			))
			{
				tests::Expect(
					result,
					packet.lastProcessedInputSequence == 1001,
					"SnapshotBroadcastBuilder: first peer last input sequence"
				);
			}
			else if (IsSameRemoteAddress(
				task.remoteAddress,
				MakeRemoteAddress(secondEndpointKey)
			))
			{
				tests::Expect(
					result,
					packet.lastProcessedInputSequence == 1002,
					"SnapshotBroadcastBuilder: second peer last input sequence"
				);
			}
			else
			{
				tests::Expect(
					result,
					false,
					"SnapshotBroadcastBuilder: unexpected player snapshot target"
				);
			}
		}
	}

	void RunBuildPlayerSnapshotMaxPlayerLimitTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		for (std::size_t index = 0;
			index < common::packet::maxPlayersPerSnapshot + 3;
			++index)
		{
			const EndpointKey endpointKey =
				MakeEndpointKey(static_cast<std::uint32_t>(index + 1));

			const common::game::PlayerId playerId =
				static_cast<common::game::PlayerId>(index + 1);

			AddRoomMember(roomTable, 1, endpointKey);

			AddPeer(
				peerTable,
				endpointKey,
				playerId,
				1,
				true,
				static_cast<std::uint32_t>(index + 1)
			);

			AddPlayer(
				gameWorld,
				playerId,
				static_cast<float>(index),
				static_cast<float>(index),
				3,
				false
			);
		}

		const std::vector<server::protocol::PlayerSnapshotTask> taskList =
			builder.BuildPlayerSnapshotTasks(
				roomTable,
				peerTable,
				gameWorld
			);

		tests::Expect(
			result,
			taskList.size() == common::packet::maxPlayersPerSnapshot + 3,
			"SnapshotBroadcastBuilder: max player limit task count"
		);

		if (!taskList.empty())
		{
			tests::Expect(
				result,
				taskList.front().snapshotPacket.playerCount
				== common::packet::maxPlayersPerSnapshot,
				"SnapshotBroadcastBuilder: max player limit packet count"
			);
		}
	}

	void RunBuildBulletSnapshotEmptyRoomCreatesEmptyChunkTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		AdvanceServerTick(gameWorld, 20);

		const EndpointKey endpointKey = MakeEndpointKey(1);

		AddRoomMember(roomTable, 1, endpointKey);
		AddPeer(peerTable, endpointKey, 101, 1, true, 1);

		const std::vector<server::protocol::BulletSnapshotTask> taskList =
			builder.BuildBulletSnapshotTasks(
				roomTable,
				peerTable,
				gameWorld
			);

		tests::Expect(
			result,
			taskList.size() == 1,
			"SnapshotBroadcastBuilder: empty bullet room task count"
		);

		if (taskList.empty())
		{
			return;
		}

		const server::protocol::BulletSnapshotTask& task =
			taskList.front();

		tests::Expect(result, task.remoteAddressList.size() == 1, "SnapshotBroadcastBuilder: empty bullet remote count");
		tests::Expect(result, task.snapshotPacket.serverTick == 20, "SnapshotBroadcastBuilder: empty bullet serverTick");
		tests::Expect(result, task.snapshotPacket.roomId == 1, "SnapshotBroadcastBuilder: empty bullet roomId");
		tests::Expect(result, task.snapshotPacket.chunkIndex == 0, "SnapshotBroadcastBuilder: empty bullet chunkIndex");
		tests::Expect(result, task.snapshotPacket.chunkCount == 1, "SnapshotBroadcastBuilder: empty bullet chunkCount");
		tests::Expect(result, task.snapshotPacket.bulletCount == 0, "SnapshotBroadcastBuilder: empty bullet count");
	}

	void RunBuildBulletSnapshotRoomFilterTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey roomOneEndpointKey = MakeEndpointKey(1);
		const EndpointKey roomTwoEndpointKey = MakeEndpointKey(2);

		AddRoomMember(roomTable, 1, roomOneEndpointKey);
		AddRoomMember(roomTable, 2, roomTwoEndpointKey);

		AddPeer(peerTable, roomOneEndpointKey, 101, 1, true, 1);
		AddPeer(peerTable, roomTwoEndpointKey, 201, 2, true, 1);

		AddBullet(gameWorld, 1001, 1, 10.0F, 20.0F);
		AddBullet(gameWorld, 2001, 2, 30.0F, 40.0F);

		const std::vector<server::protocol::BulletSnapshotTask> taskList =
			builder.BuildBulletSnapshotTasks(
				roomTable,
				peerTable,
				gameWorld
			);

		tests::Expect(
			result,
			taskList.size() == 2,
			"SnapshotBroadcastBuilder: bullet room filter task count"
		);

		for (const server::protocol::BulletSnapshotTask& task : taskList)
		{
			if (task.snapshotPacket.roomId == 1)
			{
				tests::Expect(result, task.snapshotPacket.bulletCount == 1, "SnapshotBroadcastBuilder: room1 bullet count");

				tests::Expect(
					result,
					FindBulletStateData(task.snapshotPacket, 1001) != nullptr,
					"SnapshotBroadcastBuilder: room1 bullet included"
				);

				tests::Expect(
					result,
					FindBulletStateData(task.snapshotPacket, 2001) == nullptr,
					"SnapshotBroadcastBuilder: room2 bullet excluded from room1"
				);

				tests::Expect(
					result,
					ContainsRemoteAddress(
						task.remoteAddressList,
						MakeRemoteAddress(roomOneEndpointKey)
					),
					"SnapshotBroadcastBuilder: room1 remote included"
				);
			}
			else if (task.snapshotPacket.roomId == 2)
			{
				tests::Expect(result, task.snapshotPacket.bulletCount == 1, "SnapshotBroadcastBuilder: room2 bullet count");

				tests::Expect(
					result,
					FindBulletStateData(task.snapshotPacket, 2001) != nullptr,
					"SnapshotBroadcastBuilder: room2 bullet included"
				);

				tests::Expect(
					result,
					FindBulletStateData(task.snapshotPacket, 1001) == nullptr,
					"SnapshotBroadcastBuilder: room1 bullet excluded from room2"
				);

				tests::Expect(
					result,
					ContainsRemoteAddress(
						task.remoteAddressList,
						MakeRemoteAddress(roomTwoEndpointKey)
					),
					"SnapshotBroadcastBuilder: room2 remote included"
				);
			}
			else
			{
				tests::Expect(
					result,
					false,
					"SnapshotBroadcastBuilder: unexpected bullet room task"
				);
			}
		}
	}

	void RunBuildBulletSnapshotChunkSplitTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey endpointKey = MakeEndpointKey(1);

		AddRoomMember(roomTable, 1, endpointKey);
		AddPeer(peerTable, endpointKey, 101, 1, true, 1);

		for (std::size_t index = 0;
			index < common::packet::maxBulletsPerSnapshot + 1;
			++index)
		{
			AddBullet(
				gameWorld,
				static_cast<common::game::BulletId>(index + 1),
				1,
				static_cast<float>(index),
				static_cast<float>(index)
			);
		}

		const std::vector<server::protocol::BulletSnapshotTask> taskList =
			builder.BuildBulletSnapshotTasks(
				roomTable,
				peerTable,
				gameWorld
			);

		tests::Expect(
			result,
			taskList.size() == 2,
			"SnapshotBroadcastBuilder: bullet chunk split task count"
		);

		std::uint16_t maxChunkBulletCount = 0;
		std::uint16_t lastChunkBulletCount = 0;

		for (const server::protocol::BulletSnapshotTask& task : taskList)
		{
			tests::Expect(
				result,
				task.snapshotPacket.chunkCount == 2,
				"SnapshotBroadcastBuilder: bullet chunkCount"
			);

			if (task.snapshotPacket.chunkIndex == 0)
			{
				maxChunkBulletCount = task.snapshotPacket.bulletCount;
			}
			else if (task.snapshotPacket.chunkIndex == 1)
			{
				lastChunkBulletCount = task.snapshotPacket.bulletCount;
			}
		}

		tests::Expect(
			result,
			maxChunkBulletCount == common::packet::maxBulletsPerSnapshot,
			"SnapshotBroadcastBuilder: first bullet chunk is full"
		);

		tests::Expect(
			result,
			lastChunkBulletCount == 1,
			"SnapshotBroadcastBuilder: last bullet chunk has remainder"
		);
	}

	void RunBuildBulletSnapshotSkipsRoomWithoutRemoteAddressTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey unjoinedEndpointKey = MakeEndpointKey(1);
		const EndpointKey missingEndpointKey = MakeEndpointKey(2);

		AddRoomMember(roomTable, 1, unjoinedEndpointKey);
		AddRoomMember(roomTable, 1, missingEndpointKey);
		AddPeer(peerTable, unjoinedEndpointKey, 101, 1, false, 1);

		AddBullet(gameWorld, 1, 1, 10.0F, 20.0F);

		const std::vector<server::protocol::BulletSnapshotTask> taskList =
			builder.BuildBulletSnapshotTasks(
				roomTable,
				peerTable,
				gameWorld
			);

		tests::Expect(
			result,
			taskList.empty(),
			"SnapshotBroadcastBuilder: bullet room without remotes skipped"
		);
	}

	void RunBuildImpactEffectEmptyListCreatesNoTaskTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey endpointKey = MakeEndpointKey(1);

		AddRoomMember(roomTable, 1, endpointKey);
		AddPeer(peerTable, endpointKey, 101, 1, true, 1);

		const std::vector<server::protocol::ImpactEffectTask> taskList =
			builder.BuildImpactEffectTasks(
				roomTable,
				peerTable,
				gameWorld
			);

		tests::Expect(
			result,
			taskList.empty(),
			"SnapshotBroadcastBuilder: empty impact list creates no task"
		);
	}

	void RunBuildImpactEffectRoomFilterTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey roomOneEndpointKey = MakeEndpointKey(1);
		const EndpointKey roomTwoEndpointKey = MakeEndpointKey(2);

		AddRoomMember(roomTable, 1, roomOneEndpointKey);
		AddRoomMember(roomTable, 2, roomTwoEndpointKey);

		AddPeer(peerTable, roomOneEndpointKey, 101, 1, true, 1);
		AddPeer(peerTable, roomTwoEndpointKey, 201, 2, true, 1);

		AddImpactEffect(
			gameWorld,
			1,
			common::game::EffectType::Impact,
			10.0F,
			20.0F
		);

		AddImpactEffect(
			gameWorld,
			2,
			common::game::EffectType::Spawn,
			30.0F,
			40.0F
		);

		const std::vector<server::protocol::ImpactEffectTask> taskList =
			builder.BuildImpactEffectTasks(
				roomTable,
				peerTable,
				gameWorld
			);

		tests::Expect(
			result,
			taskList.size() == 2,
			"SnapshotBroadcastBuilder: impact room filter task count"
		);

		for (const server::protocol::ImpactEffectTask& task : taskList)
		{
			if (task.effectPacket.roomId == 1)
			{
				tests::Expect(result, task.effectPacket.effectCount == 1, "SnapshotBroadcastBuilder: room1 impact count");

				tests::Expect(
					result,
					task.effectPacket.effects[0].effectType
					== common::game::EffectType::Impact,
					"SnapshotBroadcastBuilder: room1 impact type"
				);

				tests::Expect(
					result,
					ContainsRemoteAddress(
						task.remoteAddressList,
						MakeRemoteAddress(roomOneEndpointKey)
					),
					"SnapshotBroadcastBuilder: room1 impact remote"
				);
			}
			else if (task.effectPacket.roomId == 2)
			{
				tests::Expect(result, task.effectPacket.effectCount == 1, "SnapshotBroadcastBuilder: room2 impact count");

				tests::Expect(
					result,
					task.effectPacket.effects[0].effectType
					== common::game::EffectType::Spawn,
					"SnapshotBroadcastBuilder: room2 impact type"
				);

				tests::Expect(
					result,
					ContainsRemoteAddress(
						task.remoteAddressList,
						MakeRemoteAddress(roomTwoEndpointKey)
					),
					"SnapshotBroadcastBuilder: room2 impact remote"
				);
			}
			else
			{
				tests::Expect(
					result,
					false,
					"SnapshotBroadcastBuilder: unexpected impact room task"
				);
			}
		}
	}

	void RunBuildImpactEffectChunkSplitTest(
		tests::DebugTestResult& result
	)
	{
		server::protocol::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey endpointKey = MakeEndpointKey(1);

		AddRoomMember(roomTable, 1, endpointKey);
		AddPeer(peerTable, endpointKey, 101, 1, true, 1);

		for (std::size_t index = 0;
			index < common::packet::maxImpactEffectsPerPacket + 1;
			++index)
		{
			AddImpactEffect(
				gameWorld,
				1,
				common::game::EffectType::Impact,
				static_cast<float>(index),
				static_cast<float>(index)
			);
		}

		const std::vector<server::protocol::ImpactEffectTask> taskList =
			builder.BuildImpactEffectTasks(
				roomTable,
				peerTable,
				gameWorld
			);

		tests::Expect(
			result,
			taskList.size() == 2,
			"SnapshotBroadcastBuilder: impact chunk split task count"
		);

		std::uint16_t maxChunkEffectCount = 0;
		std::uint16_t lastChunkEffectCount = 0;

		for (const server::protocol::ImpactEffectTask& task : taskList)
		{
			tests::Expect(
				result,
				task.effectPacket.chunkCount == 2,
				"SnapshotBroadcastBuilder: impact chunkCount"
			);

			if (task.effectPacket.chunkIndex == 0)
			{
				maxChunkEffectCount = task.effectPacket.effectCount;
			}
			else if (task.effectPacket.chunkIndex == 1)
			{
				lastChunkEffectCount = task.effectPacket.effectCount;
			}
		}

		tests::Expect(
			result,
			maxChunkEffectCount
			== common::packet::maxImpactEffectsPerPacket,
			"SnapshotBroadcastBuilder: first impact chunk is full"
		);

		tests::Expect(
			result,
			lastChunkEffectCount == 1,
			"SnapshotBroadcastBuilder: last impact chunk has remainder"
		);
	}
}

namespace tests::server
{
	tests::DebugTestResult RunSnapshotBroadcastBuilderTests()
	{
		tests::DebugTestResult result{};

		RunBuildPlayerSnapshotTasksTest(result);
		RunBuildPlayerSnapshotMaxPlayerLimitTest(result);
		RunBuildBulletSnapshotEmptyRoomCreatesEmptyChunkTest(result);
		RunBuildBulletSnapshotRoomFilterTest(result);
		RunBuildBulletSnapshotChunkSplitTest(result);
		RunBuildBulletSnapshotSkipsRoomWithoutRemoteAddressTest(result);
		RunBuildImpactEffectEmptyListCreatesNoTaskTest(result);
		RunBuildImpactEffectRoomFilterTest(result);
		RunBuildImpactEffectChunkSplitTest(result);

		return result;
	}
}