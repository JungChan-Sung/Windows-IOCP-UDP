#include "SnapshotBroadcastBuilderTests.h"

#include <WinSock2.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <Common/Diagnostics/DebugTestResult.h>
#include <Common/Packet/GamePacket.h>
#include <Common/Packet/PacketConstants.h>

#include <Server/Game/BulletState.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/ImpactEffectState.h>
#include <Server/Game/PlayerState.h>
#include <Server/Net/PeerState.h>
#include <Server/Net/SnapshotBroadcastBuilder.h>
#include <Server/Net/SnapshotBroadcastTask.h>

namespace
{
	using EndpointKey = common::net::EndpointKey;
	using EndpointKeyList = std::vector<EndpointKey>;
	using PeerTable = server::net::SnapshotBroadcastBuilder::PeerTable;
	using RoomTable = server::net::SnapshotBroadcastBuilder::RoomTable;
	using RoomMemberSet = server::net::SnapshotBroadcastBuilder::RoomMemberSet;

	[[nodiscard]] EndpointKey MakeEndpointKey(std::uint32_t index) noexcept
	{
		EndpointKey endpointKey{};
		endpointKey.address = 0x7F000001 + index;
		endpointKey.port = static_cast<std::uint16_t>(10000 + index);
		return endpointKey;
	}

	[[nodiscard]] sockaddr_in MakeRemoteAddress(const EndpointKey& endpointKey) noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_addr.S_un.S_addr = endpointKey.address;
		remoteAddress.sin_port = endpointKey.port;
		return remoteAddress;
	}

	[[nodiscard]] bool IsSameRemoteAddress(const sockaddr_in& lhs, const sockaddr_in& rhs) noexcept
	{
		return lhs.sin_family == rhs.sin_family
			&& lhs.sin_addr.S_un.S_addr == rhs.sin_addr.S_un.S_addr
			&& lhs.sin_port == rhs.sin_port;
	}

	[[nodiscard]] bool ContainsRemoteAddress(const server::net::RemoteAddressList& remoteAddressList, const sockaddr_in& remoteAddress)
	{
		const auto remoteAddressIterator = std::ranges::find_if(
			remoteAddressList,
			[&remoteAddress](const sockaddr_in& currentRemoteAddress)
			{
				return IsSameRemoteAddress(currentRemoteAddress, remoteAddress);
			}
		);

		return remoteAddressIterator != remoteAddressList.end();
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

	void AddRoomMember(RoomTable& roomTable, common::game::RoomId roomId, const EndpointKey& endpointKey)
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
		server::net::PeerState peerState{};
		peerState.remoteAddress = MakeRemoteAddress(endpointKey);
		peerState.endpointKey = endpointKey;
		peerState.playerId = playerId;
		peerState.roomId = roomId;
		peerState.isJoined = isJoined;
		peerState.lastInputSequence = lastInputSequence;

		peerTable.insert_or_assign(endpointKey, peerState);
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
		playerState.respawnRemainingSeconds = isDead ? 2.0F : 0.0F;
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
		common::packet::EffectType effectType,
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

	void AdvanceServerTick(server::game::GameWorld& gameWorld, std::uint32_t tickCount)
	{
		for (std::uint32_t tick = 0; tick < tickCount; ++tick)
		{
			gameWorld.AdvanceServerTick();
		}
	}

	void RunBuildPlayerSnapshotTasksTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::SnapshotBroadcastBuilder builder;
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

		const std::vector<server::net::PlayerSnapshotTask> taskList = builder.BuildPlayerSnapshotTasks(
			roomTable,
			peerTable,
			gameWorld
		);

		common::diagnostics::Expect(result, taskList.size() == 2, "SnapshotBroadcastBuilder: player task count");

		for (const server::net::PlayerSnapshotTask& task : taskList)
		{
			const common::packet::PlayerSnapshotPacket& packet = task.snapshotPacket;

			common::diagnostics::Expect(result, packet.serverTick == 10, "SnapshotBroadcastBuilder: player packet serverTick");
			common::diagnostics::Expect(result, packet.roomId == 1, "SnapshotBroadcastBuilder: player packet roomId");
			common::diagnostics::Expect(result, packet.playerCount == 2, "SnapshotBroadcastBuilder: player packet playerCount");

			const common::packet::PlayerStateData* firstPlayerData = FindPlayerStateData(packet, 101);
			const common::packet::PlayerStateData* secondPlayerData = FindPlayerStateData(packet, 102);
			const common::packet::PlayerStateData* unjoinedPlayerData = FindPlayerStateData(packet, 103);

			common::diagnostics::Expect(result, firstPlayerData != nullptr, "SnapshotBroadcastBuilder: first player included");
			common::diagnostics::Expect(result, secondPlayerData != nullptr, "SnapshotBroadcastBuilder: second player included");
			common::diagnostics::Expect(result, unjoinedPlayerData == nullptr, "SnapshotBroadcastBuilder: unjoined player excluded");

			if (firstPlayerData != nullptr)
			{
				common::diagnostics::Expect(result, firstPlayerData->x == 10.0F, "SnapshotBroadcastBuilder: first player x");
				common::diagnostics::Expect(result, firstPlayerData->y == 20.0F, "SnapshotBroadcastBuilder: first player y");
				common::diagnostics::Expect(result, firstPlayerData->hp == 3, "SnapshotBroadcastBuilder: first player hp");
				common::diagnostics::Expect(result, firstPlayerData->isDead == 0, "SnapshotBroadcastBuilder: first player alive flag");
			}

			if (secondPlayerData != nullptr)
			{
				common::diagnostics::Expect(result, secondPlayerData->hp == 0, "SnapshotBroadcastBuilder: second player hp");
				common::diagnostics::Expect(result, secondPlayerData->isDead == 1, "SnapshotBroadcastBuilder: second player dead flag");
			}

			if (IsSameRemoteAddress(task.remoteAddress, MakeRemoteAddress(firstEndpointKey)))
			{
				common::diagnostics::Expect(result, packet.lastProcessedInputSequence == 1001,
					"SnapshotBroadcastBuilder: first peer last input sequence");
			}
			else if (IsSameRemoteAddress(task.remoteAddress, MakeRemoteAddress(secondEndpointKey)))
			{
				common::diagnostics::Expect(result, packet.lastProcessedInputSequence == 1002,
					"SnapshotBroadcastBuilder: second peer last input sequence");
			}
			else
			{
				common::diagnostics::Expect(result, false, "SnapshotBroadcastBuilder: unexpected player snapshot target");
			}
		}
	}

	void RunBuildPlayerSnapshotMaxPlayerLimitTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		for (std::size_t index = 0; index < common::packet::maxPlayersPerSnapshot + 3; ++index)
		{
			const EndpointKey endpointKey = MakeEndpointKey(static_cast<std::uint32_t>(index + 1));
			const common::game::PlayerId playerId = static_cast<common::game::PlayerId>(index + 1);

			AddRoomMember(roomTable, 1, endpointKey);
			AddPeer(peerTable, endpointKey, playerId, 1, true, static_cast<std::uint32_t>(index + 1));
			AddPlayer(gameWorld, playerId, static_cast<float>(index), static_cast<float>(index), 3, false);
		}

		const std::vector<server::net::PlayerSnapshotTask> taskList = builder.BuildPlayerSnapshotTasks(
			roomTable,
			peerTable,
			gameWorld
		);

		common::diagnostics::Expect(result, taskList.size() == common::packet::maxPlayersPerSnapshot + 3,
			"SnapshotBroadcastBuilder: max player limit task count");

		if (!taskList.empty())
		{
			common::diagnostics::Expect(result, taskList.front().snapshotPacket.playerCount == common::packet::maxPlayersPerSnapshot,
				"SnapshotBroadcastBuilder: max player limit packet count");
		}
	}

	void RunBuildBulletSnapshotEmptyRoomCreatesEmptyChunkTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		AdvanceServerTick(gameWorld, 20);

		const EndpointKey endpointKey = MakeEndpointKey(1);
		AddRoomMember(roomTable, 1, endpointKey);
		AddPeer(peerTable, endpointKey, 101, 1, true, 1);

		const std::vector<server::net::BulletSnapshotTask> taskList = builder.BuildBulletSnapshotTasks(
			roomTable,
			peerTable,
			gameWorld
		);

		common::diagnostics::Expect(result, taskList.size() == 1, "SnapshotBroadcastBuilder: empty bullet room task count");

		if (taskList.empty())
		{
			return;
		}

		const server::net::BulletSnapshotTask& task = taskList.front();
		common::diagnostics::Expect(result, task.remoteAddressList.size() == 1, "SnapshotBroadcastBuilder: empty bullet remote count");
		common::diagnostics::Expect(result, task.snapshotPacket.serverTick == 20, "SnapshotBroadcastBuilder: empty bullet serverTick");
		common::diagnostics::Expect(result, task.snapshotPacket.roomId == 1, "SnapshotBroadcastBuilder: empty bullet roomId");
		common::diagnostics::Expect(result, task.snapshotPacket.chunkIndex == 0, "SnapshotBroadcastBuilder: empty bullet chunkIndex");
		common::diagnostics::Expect(result, task.snapshotPacket.chunkCount == 1, "SnapshotBroadcastBuilder: empty bullet chunkCount");
		common::diagnostics::Expect(result, task.snapshotPacket.bulletCount == 0, "SnapshotBroadcastBuilder: empty bullet count");
	}

	void RunBuildBulletSnapshotRoomFilterTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::SnapshotBroadcastBuilder builder;
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

		const std::vector<server::net::BulletSnapshotTask> taskList = builder.BuildBulletSnapshotTasks(
			roomTable,
			peerTable,
			gameWorld
		);

		common::diagnostics::Expect(result, taskList.size() == 2, "SnapshotBroadcastBuilder: bullet room filter task count");

		for (const server::net::BulletSnapshotTask& task : taskList)
		{
			if (task.snapshotPacket.roomId == 1)
			{
				common::diagnostics::Expect(result, task.snapshotPacket.bulletCount == 1, "SnapshotBroadcastBuilder: room1 bullet count");
				common::diagnostics::Expect(result, FindBulletStateData(task.snapshotPacket, 1001) != nullptr,
					"SnapshotBroadcastBuilder: room1 bullet included");
				common::diagnostics::Expect(result, FindBulletStateData(task.snapshotPacket, 2001) == nullptr,
					"SnapshotBroadcastBuilder: room2 bullet excluded from room1");
				common::diagnostics::Expect(result, ContainsRemoteAddress(task.remoteAddressList, MakeRemoteAddress(roomOneEndpointKey)),
					"SnapshotBroadcastBuilder: room1 remote included");
			}
			else if (task.snapshotPacket.roomId == 2)
			{
				common::diagnostics::Expect(result, task.snapshotPacket.bulletCount == 1, "SnapshotBroadcastBuilder: room2 bullet count");
				common::diagnostics::Expect(result, FindBulletStateData(task.snapshotPacket, 2001) != nullptr,
					"SnapshotBroadcastBuilder: room2 bullet included");
				common::diagnostics::Expect(result, FindBulletStateData(task.snapshotPacket, 1001) == nullptr,
					"SnapshotBroadcastBuilder: room1 bullet excluded from room2");
				common::diagnostics::Expect(result, ContainsRemoteAddress(task.remoteAddressList, MakeRemoteAddress(roomTwoEndpointKey)),
					"SnapshotBroadcastBuilder: room2 remote included");
			}
			else
			{
				common::diagnostics::Expect(result, false, "SnapshotBroadcastBuilder: unexpected bullet room task");
			}
		}
	}

	void RunBuildBulletSnapshotChunkSplitTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey endpointKey = MakeEndpointKey(1);
		AddRoomMember(roomTable, 1, endpointKey);
		AddPeer(peerTable, endpointKey, 101, 1, true, 1);

		for (std::size_t index = 0; index < common::packet::maxBulletsPerSnapshot + 1; ++index)
		{
			AddBullet(
				gameWorld,
				static_cast<common::game::BulletId>(index + 1),
				1,
				static_cast<float>(index),
				static_cast<float>(index)
			);
		}

		const std::vector<server::net::BulletSnapshotTask> taskList = builder.BuildBulletSnapshotTasks(
			roomTable,
			peerTable,
			gameWorld
		);

		common::diagnostics::Expect(result, taskList.size() == 2, "SnapshotBroadcastBuilder: bullet chunk split task count");

		std::uint16_t maxChunkBulletCount = 0;
		std::uint16_t lastChunkBulletCount = 0;

		for (const server::net::BulletSnapshotTask& task : taskList)
		{
			common::diagnostics::Expect(result, task.snapshotPacket.chunkCount == 2, "SnapshotBroadcastBuilder: bullet chunkCount");

			if (task.snapshotPacket.chunkIndex == 0)
			{
				maxChunkBulletCount = task.snapshotPacket.bulletCount;
			}
			else if (task.snapshotPacket.chunkIndex == 1)
			{
				lastChunkBulletCount = task.snapshotPacket.bulletCount;
			}
		}

		common::diagnostics::Expect(result, maxChunkBulletCount == common::packet::maxBulletsPerSnapshot,
			"SnapshotBroadcastBuilder: first bullet chunk is full");
		common::diagnostics::Expect(result, lastChunkBulletCount == 1, "SnapshotBroadcastBuilder: last bullet chunk has remainder");
	}

	void RunBuildBulletSnapshotSkipsRoomWithoutRemoteAddressTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey unjoinedEndpointKey = MakeEndpointKey(1);
		const EndpointKey missingEndpointKey = MakeEndpointKey(2);

		AddRoomMember(roomTable, 1, unjoinedEndpointKey);
		AddRoomMember(roomTable, 1, missingEndpointKey);
		AddPeer(peerTable, unjoinedEndpointKey, 101, 1, false, 1);

		AddBullet(gameWorld, 1, 1, 10.0F, 20.0F);

		const std::vector<server::net::BulletSnapshotTask> taskList = builder.BuildBulletSnapshotTasks(
			roomTable,
			peerTable,
			gameWorld
		);

		common::diagnostics::Expect(result, taskList.empty(), "SnapshotBroadcastBuilder: bullet room without remotes skipped");
	}

	void RunBuildImpactEffectEmptyListCreatesNoTaskTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey endpointKey = MakeEndpointKey(1);
		AddRoomMember(roomTable, 1, endpointKey);
		AddPeer(peerTable, endpointKey, 101, 1, true, 1);

		const std::vector<server::net::ImpactEffectTask> taskList = builder.BuildImpactEffectTasks(
			roomTable,
			peerTable,
			gameWorld
		);

		common::diagnostics::Expect(result, taskList.empty(), "SnapshotBroadcastBuilder: empty impact list creates no task");
	}

	void RunBuildImpactEffectRoomFilterTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey roomOneEndpointKey = MakeEndpointKey(1);
		const EndpointKey roomTwoEndpointKey = MakeEndpointKey(2);

		AddRoomMember(roomTable, 1, roomOneEndpointKey);
		AddRoomMember(roomTable, 2, roomTwoEndpointKey);
		AddPeer(peerTable, roomOneEndpointKey, 101, 1, true, 1);
		AddPeer(peerTable, roomTwoEndpointKey, 201, 2, true, 1);

		AddImpactEffect(gameWorld, 1, common::packet::EffectType::Impact, 10.0F, 20.0F);
		AddImpactEffect(gameWorld, 2, common::packet::EffectType::Spawn, 30.0F, 40.0F);

		const std::vector<server::net::ImpactEffectTask> taskList = builder.BuildImpactEffectTasks(
			roomTable,
			peerTable,
			gameWorld
		);

		common::diagnostics::Expect(result, taskList.size() == 2, "SnapshotBroadcastBuilder: impact room filter task count");

		for (const server::net::ImpactEffectTask& task : taskList)
		{
			if (task.effectPacket.roomId == 1)
			{
				common::diagnostics::Expect(result, task.effectPacket.effectCount == 1, "SnapshotBroadcastBuilder: room1 impact count");
				common::diagnostics::Expect(result, task.effectPacket.effects[0].effectType == common::packet::EffectType::Impact,
					"SnapshotBroadcastBuilder: room1 impact type");
				common::diagnostics::Expect(result, ContainsRemoteAddress(task.remoteAddressList, MakeRemoteAddress(roomOneEndpointKey)),
					"SnapshotBroadcastBuilder: room1 impact remote");
			}
			else if (task.effectPacket.roomId == 2)
			{
				common::diagnostics::Expect(result, task.effectPacket.effectCount == 1, "SnapshotBroadcastBuilder: room2 impact count");
				common::diagnostics::Expect(result, task.effectPacket.effects[0].effectType == common::packet::EffectType::Spawn,
					"SnapshotBroadcastBuilder: room2 impact type");
				common::diagnostics::Expect(result, ContainsRemoteAddress(task.remoteAddressList, MakeRemoteAddress(roomTwoEndpointKey)),
					"SnapshotBroadcastBuilder: room2 impact remote");
			}
			else
			{
				common::diagnostics::Expect(result, false, "SnapshotBroadcastBuilder: unexpected impact room task");
			}
		}
	}

	void RunBuildImpactEffectChunkSplitTest(common::diagnostics::DebugTestResult& result)
	{
		server::net::SnapshotBroadcastBuilder builder;
		server::game::GameWorld gameWorld;
		PeerTable peerTable;
		RoomTable roomTable;

		const EndpointKey endpointKey = MakeEndpointKey(1);
		AddRoomMember(roomTable, 1, endpointKey);
		AddPeer(peerTable, endpointKey, 101, 1, true, 1);

		for (std::size_t index = 0; index < common::packet::maxImpactEffectsPerPacket + 1; ++index)
		{
			AddImpactEffect(
				gameWorld,
				1,
				common::packet::EffectType::Impact,
				static_cast<float>(index),
				static_cast<float>(index)
			);
		}

		const std::vector<server::net::ImpactEffectTask> taskList = builder.BuildImpactEffectTasks(
			roomTable,
			peerTable,
			gameWorld
		);

		common::diagnostics::Expect(result, taskList.size() == 2, "SnapshotBroadcastBuilder: impact chunk split task count");

		std::uint16_t maxChunkEffectCount = 0;
		std::uint16_t lastChunkEffectCount = 0;

		for (const server::net::ImpactEffectTask& task : taskList)
		{
			common::diagnostics::Expect(result, task.effectPacket.chunkCount == 2, "SnapshotBroadcastBuilder: impact chunkCount");

			if (task.effectPacket.chunkIndex == 0)
			{
				maxChunkEffectCount = task.effectPacket.effectCount;
			}
			else if (task.effectPacket.chunkIndex == 1)
			{
				lastChunkEffectCount = task.effectPacket.effectCount;
			}
		}

		common::diagnostics::Expect(result, maxChunkEffectCount == common::packet::maxImpactEffectsPerPacket,
			"SnapshotBroadcastBuilder: first impact chunk is full");
		common::diagnostics::Expect(result, lastChunkEffectCount == 1, "SnapshotBroadcastBuilder: last impact chunk has remainder");
	}
}

namespace tests::server
{
	common::diagnostics::DebugTestResult RunSnapshotBroadcastBuilderTests()
	{
		common::diagnostics::DebugTestResult result{};

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