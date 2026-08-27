#include "SnapshotBroadcastBuilder.h"

#include <algorithm>
#include <utility>

namespace server::protocol
{
	std::vector<PlayerSnapshotTask> SnapshotBroadcastBuilder::BuildPlayerSnapshotTasks(const SnapshotBroadcastContext& context) const
	{
		std::vector<PlayerSnapshotTask> playerSnapshotTaskList;

		for (const SnapshotRoomContext& roomContext : context.roomContextList)
		{
			common::packet::PlayerSnapshotPacket snapshotBase{};
			FillPlayerSnapshotBase(snapshotBase, roomContext, context.serverTick);

			for (const SnapshotPeerContext& peerContext : roomContext.peerContextList)
			{
				PlayerSnapshotTask task{};
				task.endpointKey = peerContext.endpointKey;
				task.snapshotPacket = snapshotBase;
				task.snapshotPacket.lastProcessedInputSequence = peerContext.lastProcessedInputSequence;

				playerSnapshotTaskList.push_back(std::move(task));
			}
		}

		return playerSnapshotTaskList;
	}

	std::vector<BulletSnapshotTask> SnapshotBroadcastBuilder::BuildBulletSnapshotTasks(const SnapshotBroadcastContext& context) const
	{
		std::vector<BulletSnapshotTask> bulletSnapshotTaskList;

		for (const SnapshotRoomContext& roomContext : context.roomContextList)
		{
			if (roomContext.peerContextList.empty())
			{
				continue;
			}

			const EndpointKeyList endpointKeyList = BuildEndpointKeyList(roomContext.peerContextList);

			const std::size_t maxBulletPerChunk = common::packet::maxBulletsPerSnapshot;
			const std::size_t chunkCount = std::max(1ULL, (roomContext.bulletStateContextList.size() + maxBulletPerChunk - 1) / maxBulletPerChunk);
			for (std::size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
			{
				BulletSnapshotTask task{};
				task.endpointKeyList = endpointKeyList;
				task.snapshotPacket.serverTick = context.serverTick;
				task.snapshotPacket.roomId = roomContext.roomId;
				task.snapshotPacket.chunkIndex = static_cast<std::uint16_t>(chunkIndex);
				task.snapshotPacket.chunkCount = static_cast<std::uint16_t>(chunkCount);

				const std::size_t beginIndex = chunkIndex * maxBulletPerChunk;
				const std::size_t endIndex = std::min(beginIndex + maxBulletPerChunk, roomContext.bulletStateContextList.size());
				for (std::size_t index = beginIndex; index < endIndex; ++index)
				{
					const SnapshotBulletStateContext& bulletContext = roomContext.bulletStateContextList[index];
					common::packet::BulletStateData& bulletData = task.snapshotPacket.bullets[task.snapshotPacket.bulletCount];

					bulletData.bulletId = bulletContext.bulletId;
					bulletData.x = bulletContext.x;
					bulletData.y = bulletContext.y;

					++task.snapshotPacket.bulletCount;
				}

				bulletSnapshotTaskList.push_back(std::move(task));
			}
		}

		return bulletSnapshotTaskList;
	}

	std::vector<ImpactEffectTask> SnapshotBroadcastBuilder::BuildImpactEffectTasks(const SnapshotBroadcastContext& context) const
	{
		std::vector<ImpactEffectTask> impactEffectTaskList;

		for (const SnapshotRoomContext& roomContext : context.roomContextList)
		{
			if (roomContext.peerContextList.empty() || roomContext.impactEffectContextList.empty())
			{
				continue;
			}

			const EndpointKeyList endpointKeyList = BuildEndpointKeyList(roomContext.peerContextList);

			const std::size_t maxEffectsPerChunk = common::packet::maxImpactEffectsPerPacket;
			const std::size_t chunkCount = (roomContext.impactEffectContextList.size() + maxEffectsPerChunk - 1) / maxEffectsPerChunk;
			for (std::size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
			{
				ImpactEffectTask task{};
				task.endpointKeyList = endpointKeyList;
				task.effectPacket.serverTick = context.serverTick;
				task.effectPacket.roomId = roomContext.roomId;
				task.effectPacket.chunkIndex = static_cast<std::uint16_t>(chunkIndex);
				task.effectPacket.chunkCount = static_cast<std::uint16_t>(chunkCount);

				const std::size_t beginIndex = chunkIndex * maxEffectsPerChunk;
				const std::size_t endIndex =
					std::min(beginIndex + maxEffectsPerChunk, roomContext.impactEffectContextList.size());

				for (std::size_t index = beginIndex; index < endIndex; ++index)
				{
					const SnapshotImpactEffectContext& effectContext = roomContext.impactEffectContextList[index];
					common::packet::ImpactEffectData& effectData = task.effectPacket.effects[task.effectPacket.effectCount];

					effectData.effectType = effectContext.effectType;
					effectData.x = effectContext.x;
					effectData.y = effectContext.y;

					++task.effectPacket.effectCount;
				}

				impactEffectTaskList.push_back(std::move(task));
			}
		}

		return impactEffectTaskList;
	}

	EndpointKeyList SnapshotBroadcastBuilder::BuildEndpointKeyList(const SnapshotPeerContextList& peerContextList) const
	{
		EndpointKeyList endpointKeyList;
		endpointKeyList.reserve(peerContextList.size());

		for (const SnapshotPeerContext& peerContext : peerContextList)
		{
			endpointKeyList.push_back(peerContext.endpointKey);
		}

		return endpointKeyList;
	}

	void SnapshotBroadcastBuilder::FillPlayerSnapshotBase(common::packet::PlayerSnapshotPacket& snapshotPacket, const SnapshotRoomContext& roomContext, std::uint32_t serverTick) const
	{
		snapshotPacket.serverTick = serverTick;
		snapshotPacket.roomId = roomContext.roomId;

		for (const SnapshotPlayerStateContext& playerContext : roomContext.playerStateContextList)
		{
			if (snapshotPacket.playerCount >= snapshotPacket.players.size())
			{
				break;
			}

			common::packet::PlayerStateData& playerData = snapshotPacket.players[snapshotPacket.playerCount];

			playerData.playerId = playerContext.playerId;
			playerData.x = playerContext.x;
			playerData.y = playerContext.y;
			playerData.hp = playerContext.hp;
			playerData.isDead = playerContext.isDead ? 1 : 0;
			playerData.killCount = playerContext.killCount;
			playerData.deathCount = playerContext.deathCount;
			playerData.respawnRemainingSeconds = playerContext.respawnRemainingSeconds;
			playerData.invincibilityRemainingSeconds = playerContext.invincibilityRemainingSeconds;
			playerData.hitFlashRemainingSeconds = playerContext.hitFlashRemainingSeconds;

			++snapshotPacket.playerCount;
		}
	}
}