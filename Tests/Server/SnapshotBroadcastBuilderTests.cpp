#include "SnapshotBroadcastBuilderTests.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <Common/Net/EndpointKey.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketConstants.h>

#include <Server/Protocol/SnapshotBroadcastBuilder.h>
#include <Server/Protocol/SnapshotBroadcastContext.h>
#include <Server/Protocol/SnapshotBroadcastTask.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using EndpointKey = common::net::EndpointKey;
	using EndpointKeyList = server::protocol::EndpointKeyList;
	using SnapshotBroadcastContext = server::protocol::SnapshotBroadcastContext;
	using SnapshotRoomContext = server::protocol::SnapshotRoomContext;

	[[nodiscard]] constexpr EndpointKey MakeEndpointKey(std::uint32_t index) noexcept
	{
		return EndpointKey{
			.address = 0x7F000001 + index,
			.port = static_cast<std::uint16_t>(10000 + index),
		};
	}

	[[nodiscard]] bool ContainsEndpointKey(
		const EndpointKeyList& endpointKeyList,
		const EndpointKey& endpointKey
	)
	{
		return std::ranges::find(endpointKeyList, endpointKey) != endpointKeyList.end();
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

	[[nodiscard]] SnapshotRoomContext& AddRoom(
		SnapshotBroadcastContext& context,
		common::game::RoomId roomId
	)
	{
		context.roomContextList.push_back(
			SnapshotRoomContext{
				.roomId = roomId,
			}
			);

		return context.roomContextList.back();
	}

	void AddPeer(
		SnapshotRoomContext& roomContext,
		const EndpointKey& endpointKey,
		common::game::PlayerId playerId,
		std::uint32_t lastProcessedInputSequence
	)
	{
		roomContext.peerContextList.push_back(
			server::protocol::SnapshotPeerContext{
				.endpointKey = endpointKey,
				.playerId = playerId,
				.lastProcessedInputSequence = lastProcessedInputSequence,
			}
			);
	}

	void AddPlayer(
		SnapshotRoomContext& roomContext,
		common::game::PlayerId playerId,
		float x,
		float y,
		int hp,
		bool isDead
	)
	{
		roomContext.playerStateContextList.push_back(
			server::protocol::SnapshotPlayerStateContext{
				.playerId = playerId,
				.x = x,
				.y = y,
				.hp = hp,
				.isDead = isDead,
				.killCount = playerId * 10,
				.deathCount = playerId,
				.respawnRemainingSeconds = isDead ? 2.0F : 0.0F,
				.invincibilityRemainingSeconds = 1.0F,
				.hitFlashRemainingSeconds = 0.25F,
			}
			);
	}

	void AddBullet(
		SnapshotRoomContext& roomContext,
		common::game::BulletId bulletId,
		float x,
		float y
	)
	{
		roomContext.bulletStateContextList.push_back(
			server::protocol::SnapshotBulletStateContext{
				.bulletId = bulletId,
				.x = x,
				.y = y,
			}
			);
	}

	void AddImpactEffect(
		SnapshotRoomContext& roomContext,
		common::game::EffectType effectType,
		float x,
		float y
	)
	{
		roomContext.impactEffectContextList.push_back(
			server::protocol::SnapshotImpactEffectContext{
				.effectType = effectType,
				.x = x,
				.y = y,
			}
			);
	}

	void RunBuildPlayerSnapshotTasksTest(tests::DebugTestResult& result)
	{
		server::protocol::SnapshotBroadcastBuilder builder;

		SnapshotBroadcastContext context{};
		context.serverTick = 10;

		const EndpointKey firstEndpointKey = MakeEndpointKey(1);
		const EndpointKey secondEndpointKey = MakeEndpointKey(2);

		SnapshotRoomContext& roomContext = AddRoom(context, 1);

		AddPeer(roomContext, firstEndpointKey, 101, 1001);
		AddPeer(roomContext, secondEndpointKey, 102, 1002);

		AddPlayer(roomContext, 101, 10.0F, 20.0F, 3, false);
		AddPlayer(roomContext, 102, 30.0F, 40.0F, 0, true);

		const std::vector<server::protocol::PlayerSnapshotTask> taskList =
			builder.BuildPlayerSnapshotTasks(context);

		tests::Expect(
			result,
			taskList.size() == 2,
			"SnapshotBroadcastBuilder: player task count"
		);

		for (const server::protocol::PlayerSnapshotTask& task : taskList)
		{
			const common::packet::PlayerSnapshotPacket& packet = task.snapshotPacket;

			tests::Expect(
				result,
				packet.serverTick == 10,
				"SnapshotBroadcastBuilder: player packet serverTick"
			);

			tests::Expect(
				result,
				packet.roomId == 1,
				"SnapshotBroadcastBuilder: player packet roomId"
			);

			tests::Expect(
				result,
				packet.playerCount == 2,
				"SnapshotBroadcastBuilder: player packet playerCount"
			);

			const common::packet::PlayerStateData* firstPlayerData =
				FindPlayerStateData(packet, 101);

			const common::packet::PlayerStateData* secondPlayerData =
				FindPlayerStateData(packet, 102);

			tests::Expect(
				result,
				firstPlayerData != nullptr,
				"SnapshotBroadcastBuilder: first player included"
			);

			tests::Expect(
				result,
				secondPlayerData != nullptr,
				"SnapshotBroadcastBuilder: second player included"
			);

			if (firstPlayerData != nullptr)
			{
				tests::Expect(
					result,
					firstPlayerData->x == 10.0F,
					"SnapshotBroadcastBuilder: first player x"
				);

				tests::Expect(
					result,
					firstPlayerData->y == 20.0F,
					"SnapshotBroadcastBuilder: first player y"
				);

				tests::Expect(
					result,
					firstPlayerData->hp == 3,
					"SnapshotBroadcastBuilder: first player hp"
				);

				tests::Expect(
					result,
					firstPlayerData->isDead == 0,
					"SnapshotBroadcastBuilder: first player alive flag"
				);

				tests::Expect(
					result,
					firstPlayerData->killCount == 1010,
					"SnapshotBroadcastBuilder: first player kill count"
				);

				tests::Expect(
					result,
					firstPlayerData->deathCount == 101,
					"SnapshotBroadcastBuilder: first player death count"
				);

				tests::Expect(
					result,
					firstPlayerData->invincibilityRemainingSeconds == 1.0F,
					"SnapshotBroadcastBuilder: first player invincibility time"
				);

				tests::Expect(
					result,
					firstPlayerData->hitFlashRemainingSeconds == 0.25F,
					"SnapshotBroadcastBuilder: first player hit flash time"
				);
			}

			if (secondPlayerData != nullptr)
			{
				tests::Expect(
					result,
					secondPlayerData->hp == 0,
					"SnapshotBroadcastBuilder: second player hp"
				);

				tests::Expect(
					result,
					secondPlayerData->isDead == 1,
					"SnapshotBroadcastBuilder: second player dead flag"
				);

				tests::Expect(
					result,
					secondPlayerData->respawnRemainingSeconds == 2.0F,
					"SnapshotBroadcastBuilder: second player respawn time"
				);
			}

			if (task.endpointKey == firstEndpointKey)
			{
				tests::Expect(
					result,
					packet.lastProcessedInputSequence == 1001,
					"SnapshotBroadcastBuilder: first peer processed input sequence"
				);
			}
			else if (task.endpointKey == secondEndpointKey)
			{
				tests::Expect(
					result,
					packet.lastProcessedInputSequence == 1002,
					"SnapshotBroadcastBuilder: second peer processed input sequence"
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

	void RunBuildPlayerSnapshotMaxPlayerLimitTest(tests::DebugTestResult& result)
	{
		server::protocol::SnapshotBroadcastBuilder builder;

		SnapshotBroadcastContext context{};
		SnapshotRoomContext& roomContext = AddRoom(context, 1);

		for (std::size_t index = 0; index < common::packet::maxPlayersPerSnapshot + 3; ++index)
		{
			const EndpointKey endpointKey =
				MakeEndpointKey(static_cast<std::uint32_t>(index + 1));

			const common::game::PlayerId playerId =
				static_cast<common::game::PlayerId>(index + 1);

			AddPeer(
				roomContext,
				endpointKey,
				playerId,
				static_cast<std::uint32_t>(index + 1)
			);

			AddPlayer(
				roomContext,
				playerId,
				static_cast<float>(index),
				static_cast<float>(index),
				3,
				false
			);
		}

		const std::vector<server::protocol::PlayerSnapshotTask> taskList =
			builder.BuildPlayerSnapshotTasks(context);

		tests::Expect(
			result,
			taskList.size() == common::packet::maxPlayersPerSnapshot + 3,
			"SnapshotBroadcastBuilder: max player limit task count"
		);

		if (!taskList.empty())
		{
			tests::Expect(
				result,
				taskList.front().snapshotPacket.playerCount ==
				common::packet::maxPlayersPerSnapshot,
				"SnapshotBroadcastBuilder: max player limit packet count"
			);
		}
	}

	void RunBuildBulletSnapshotEmptyRoomCreatesEmptyChunkTest(tests::DebugTestResult& result)
	{
		server::protocol::SnapshotBroadcastBuilder builder;

		SnapshotBroadcastContext context{};
		context.serverTick = 20;

		const EndpointKey endpointKey = MakeEndpointKey(1);

		SnapshotRoomContext& roomContext = AddRoom(context, 1);
		AddPeer(roomContext, endpointKey, 101, 1);

		const std::vector<server::protocol::BulletSnapshotTask> taskList =
			builder.BuildBulletSnapshotTasks(context);

		tests::Expect(
			result,
			taskList.size() == 1,
			"SnapshotBroadcastBuilder: empty bullet room task count"
		);

		if (taskList.empty())
		{
			return;
		}

		const server::protocol::BulletSnapshotTask& task = taskList.front();

		tests::Expect(
			result,
			task.endpointKeyList.size() == 1,
			"SnapshotBroadcastBuilder: empty bullet endpoint count"
		);

		tests::Expect(
			result,
			task.endpointKeyList.front() == endpointKey,
			"SnapshotBroadcastBuilder: empty bullet endpoint"
		);

		tests::Expect(
			result,
			task.snapshotPacket.serverTick == 20,
			"SnapshotBroadcastBuilder: empty bullet serverTick"
		);

		tests::Expect(
			result,
			task.snapshotPacket.roomId == 1,
			"SnapshotBroadcastBuilder: empty bullet roomId"
		);

		tests::Expect(
			result,
			task.snapshotPacket.chunkIndex == 0,
			"SnapshotBroadcastBuilder: empty bullet chunkIndex"
		);

		tests::Expect(
			result,
			task.snapshotPacket.chunkCount == 1,
			"SnapshotBroadcastBuilder: empty bullet chunkCount"
		);

		tests::Expect(
			result,
			task.snapshotPacket.bulletCount == 0,
			"SnapshotBroadcastBuilder: empty bullet count"
		);
	}

	void RunBuildBulletSnapshotRoomFilterTest(tests::DebugTestResult& result)
	{
		server::protocol::SnapshotBroadcastBuilder builder;

		SnapshotBroadcastContext context{};
		context.roomContextList.reserve(2);

		const EndpointKey roomOneEndpointKey = MakeEndpointKey(1);
		const EndpointKey roomTwoEndpointKey = MakeEndpointKey(2);

		SnapshotRoomContext& roomOneContext = AddRoom(context, 1);

		AddPeer(roomOneContext, roomOneEndpointKey, 101, 1);
		AddBullet(roomOneContext, 1001, 10.0F, 20.0F);

		SnapshotRoomContext& roomTwoContext = AddRoom(context, 2);

		AddPeer(roomTwoContext, roomTwoEndpointKey, 201, 1);
		AddBullet(roomTwoContext, 2001, 30.0F, 40.0F);

		const std::vector<server::protocol::BulletSnapshotTask> taskList =
			builder.BuildBulletSnapshotTasks(context);

		tests::Expect(
			result,
			taskList.size() == 2,
			"SnapshotBroadcastBuilder: bullet room filter task count"
		);

		for (const server::protocol::BulletSnapshotTask& task : taskList)
		{
			if (task.snapshotPacket.roomId == 1)
			{
				tests::Expect(
					result,
					task.snapshotPacket.bulletCount == 1,
					"SnapshotBroadcastBuilder: room1 bullet count"
				);

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
					ContainsEndpointKey(
						task.endpointKeyList,
						roomOneEndpointKey
					),
					"SnapshotBroadcastBuilder: room1 endpoint included"
				);
			}
			else if (task.snapshotPacket.roomId == 2)
			{
				tests::Expect(
					result,
					task.snapshotPacket.bulletCount == 1,
					"SnapshotBroadcastBuilder: room2 bullet count"
				);

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
					ContainsEndpointKey(
						task.endpointKeyList,
						roomTwoEndpointKey
					),
					"SnapshotBroadcastBuilder: room2 endpoint included"
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

	void RunBuildBulletSnapshotChunkSplitTest(tests::DebugTestResult& result)
	{
		server::protocol::SnapshotBroadcastBuilder builder;

		SnapshotBroadcastContext context{};

		const EndpointKey endpointKey = MakeEndpointKey(1);

		SnapshotRoomContext& roomContext = AddRoom(context, 1);
		AddPeer(roomContext, endpointKey, 101, 1);

		for (std::size_t index = 0; index < common::packet::maxBulletsPerSnapshot + 1; ++index)
		{
			AddBullet(
				roomContext,
				static_cast<common::game::BulletId>(index + 1),
				static_cast<float>(index),
				static_cast<float>(index)
			);
		}

		const std::vector<server::protocol::BulletSnapshotTask> taskList =
			builder.BuildBulletSnapshotTasks(context);

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

	void RunBuildBulletSnapshotSkipsRoomWithoutPeerTest(tests::DebugTestResult& result)
	{
		server::protocol::SnapshotBroadcastBuilder builder;

		SnapshotBroadcastContext context{};
		SnapshotRoomContext& roomContext = AddRoom(context, 1);

		AddBullet(roomContext, 1, 10.0F, 20.0F);

		const std::vector<server::protocol::BulletSnapshotTask> taskList =
			builder.BuildBulletSnapshotTasks(context);

		tests::Expect(
			result,
			taskList.empty(),
			"SnapshotBroadcastBuilder: bullet room without peers skipped"
		);
	}

	void RunBuildImpactEffectEmptyListCreatesNoTaskTest(tests::DebugTestResult& result)
	{
		server::protocol::SnapshotBroadcastBuilder builder;

		SnapshotBroadcastContext context{};

		const EndpointKey endpointKey = MakeEndpointKey(1);

		SnapshotRoomContext& roomContext = AddRoom(context, 1);
		AddPeer(roomContext, endpointKey, 101, 1);

		const std::vector<server::protocol::ImpactEffectTask> taskList =
			builder.BuildImpactEffectTasks(context);

		tests::Expect(
			result,
			taskList.empty(),
			"SnapshotBroadcastBuilder: empty impact list creates no task"
		);
	}

	void RunBuildImpactEffectRoomFilterTest(tests::DebugTestResult& result)
	{
		server::protocol::SnapshotBroadcastBuilder builder;

		SnapshotBroadcastContext context{};
		context.roomContextList.reserve(2);

		const EndpointKey roomOneEndpointKey = MakeEndpointKey(1);
		const EndpointKey roomTwoEndpointKey = MakeEndpointKey(2);

		SnapshotRoomContext& roomOneContext = AddRoom(context, 1);

		AddPeer(roomOneContext, roomOneEndpointKey, 101, 1);
		AddImpactEffect(
			roomOneContext,
			common::game::EffectType::Impact,
			10.0F,
			20.0F
		);

		SnapshotRoomContext& roomTwoContext = AddRoom(context, 2);

		AddPeer(roomTwoContext, roomTwoEndpointKey, 201, 1);
		AddImpactEffect(
			roomTwoContext,
			common::game::EffectType::Spawn,
			30.0F,
			40.0F
		);

		const std::vector<server::protocol::ImpactEffectTask> taskList =
			builder.BuildImpactEffectTasks(context);

		tests::Expect(
			result,
			taskList.size() == 2,
			"SnapshotBroadcastBuilder: impact room filter task count"
		);

		for (const server::protocol::ImpactEffectTask& task : taskList)
		{
			if (task.effectPacket.roomId == 1)
			{
				tests::Expect(
					result,
					task.effectPacket.effectCount == 1,
					"SnapshotBroadcastBuilder: room1 impact count"
				);

				tests::Expect(
					result,
					task.effectPacket.effects[0].effectType ==
					common::game::EffectType::Impact,
					"SnapshotBroadcastBuilder: room1 impact type"
				);

				tests::Expect(
					result,
					ContainsEndpointKey(
						task.endpointKeyList,
						roomOneEndpointKey
					),
					"SnapshotBroadcastBuilder: room1 impact endpoint"
				);
			}
			else if (task.effectPacket.roomId == 2)
			{
				tests::Expect(
					result,
					task.effectPacket.effectCount == 1,
					"SnapshotBroadcastBuilder: room2 impact count"
				);

				tests::Expect(
					result,
					task.effectPacket.effects[0].effectType ==
					common::game::EffectType::Spawn,
					"SnapshotBroadcastBuilder: room2 impact type"
				);

				tests::Expect(
					result,
					ContainsEndpointKey(
						task.endpointKeyList,
						roomTwoEndpointKey
					),
					"SnapshotBroadcastBuilder: room2 impact endpoint"
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

	void RunBuildImpactEffectChunkSplitTest(tests::DebugTestResult& result)
	{
		server::protocol::SnapshotBroadcastBuilder builder;

		SnapshotBroadcastContext context{};

		const EndpointKey endpointKey = MakeEndpointKey(1);

		SnapshotRoomContext& roomContext = AddRoom(context, 1);
		AddPeer(roomContext, endpointKey, 101, 1);

		for (
			std::size_t index = 0;
			index < common::packet::maxImpactEffectsPerPacket + 1;
			++index
			)
		{
			AddImpactEffect(
				roomContext,
				common::game::EffectType::Impact,
				static_cast<float>(index),
				static_cast<float>(index)
			);
		}

		const std::vector<server::protocol::ImpactEffectTask> taskList =
			builder.BuildImpactEffectTasks(context);

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
			maxChunkEffectCount == common::packet::maxImpactEffectsPerPacket,
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
	DebugTestResult RunSnapshotBroadcastBuilderTests()
	{
		DebugTestResult result{};

		RunBuildPlayerSnapshotTasksTest(result);
		RunBuildPlayerSnapshotMaxPlayerLimitTest(result);

		RunBuildBulletSnapshotEmptyRoomCreatesEmptyChunkTest(result);
		RunBuildBulletSnapshotRoomFilterTest(result);
		RunBuildBulletSnapshotChunkSplitTest(result);
		RunBuildBulletSnapshotSkipsRoomWithoutPeerTest(result);

		RunBuildImpactEffectEmptyListCreatesNoTaskTest(result);
		RunBuildImpactEffectRoomFilterTest(result);
		RunBuildImpactEffectChunkSplitTest(result);

		return result;
	}
}