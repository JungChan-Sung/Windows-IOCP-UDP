#include "SnapshotBroadcastBuilder.h"

#include <algorithm>
#include <utility>

namespace server::protocol
{
	std::vector<PlayerSnapshotTask> SnapshotBroadcastBuilder::BuildPlayerSnapshotTasks(std::span<const SnapshotRoomContext> roomContextList, const game::GameWorld& gameWorld) const
	{
		std::vector<PlayerSnapshotTask> playerSnapshotTaskList;

		const PlayerTable& playerTable = gameWorld.GetPlayerTable();
		const std::uint32_t serverTick = gameWorld.GetServerTick();

		for (const SnapshotRoomContext& roomContext : roomContextList)
		{
			common::packet::PlayerSnapshotPacket snapshotBase{};

			FillPlayerSnapshotBase(
				snapshotBase,
				roomContext.roomId,
				roomContext.peerContextList,
				playerTable,
				serverTick
			);

			for (const SnapshotPeerContext& peerContext : roomContext.peerContextList)
			{
				PlayerSnapshotTask playerSnapshotTask{};
				playerSnapshotTask.endpointKey = peerContext.endpointKey;
				playerSnapshotTask.snapshotPacket = snapshotBase;
				playerSnapshotTask.snapshotPacket.lastProcessedInputSequence = peerContext.lastInputSequence;

				playerSnapshotTaskList.push_back(std::move(playerSnapshotTask));
			}
		}

		return playerSnapshotTaskList;
	}

	std::vector<BulletSnapshotTask> SnapshotBroadcastBuilder::BuildBulletSnapshotTasks(std::span<const SnapshotRoomContext> roomContextList, const game::GameWorld& gameWorld) const
	{
		std::vector<BulletSnapshotTask> bulletSnapshotTaskList;

		const BulletStateList& bulletStateList = gameWorld.GetBulletStateList();
		const std::uint32_t serverTick = gameWorld.GetServerTick();

		for (const SnapshotRoomContext& roomContext : roomContextList)
		{
			if (roomContext.peerContextList.empty())
			{
				continue;
			}

			const EndpointKeyList endpointKeyList = BuildEndpointKeyList(roomContext.peerContextList);
			const BulletStateDataList bulletStateDataList = BuildRoomBulletStateDataList(roomContext.roomId, bulletStateList);

			const std::size_t maxBulletPerChunk = common::packet::maxBulletsPerSnapshot;
			const std::size_t chunkCount = std::max(1ULL, (bulletStateDataList.size() + maxBulletPerChunk - 1) / maxBulletPerChunk);
			for (std::size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
			{
				BulletSnapshotTask bulletSnapshotTask{};
				bulletSnapshotTask.endpointKeyList = endpointKeyList;
				bulletSnapshotTask.snapshotPacket.serverTick = serverTick;
				bulletSnapshotTask.snapshotPacket.roomId = roomContext.roomId;
				bulletSnapshotTask.snapshotPacket.chunkIndex = static_cast<std::uint16_t>(chunkIndex);
				bulletSnapshotTask.snapshotPacket.chunkCount = static_cast<std::uint16_t>(chunkCount);

				const std::size_t beginIndex = chunkIndex * maxBulletPerChunk;
				const std::size_t endIndex = std::min(beginIndex + maxBulletPerChunk, bulletStateDataList.size());

				for (std::size_t index = beginIndex; index < endIndex; ++index)
				{
					bulletSnapshotTask.snapshotPacket.bullets[bulletSnapshotTask.snapshotPacket.bulletCount] = bulletStateDataList[index];
					++bulletSnapshotTask.snapshotPacket.bulletCount;
				}

				bulletSnapshotTaskList.push_back(std::move(bulletSnapshotTask));
			}
		}

		return bulletSnapshotTaskList;
	}

	std::vector<ImpactEffectTask> SnapshotBroadcastBuilder::BuildImpactEffectTasks(std::span<const SnapshotRoomContext> roomContextList, const game::GameWorld& gameWorld) const
	{
		std::vector<ImpactEffectTask> impactEffectTaskList;

		const ImpactEffectStateList& impactEffectStateList = gameWorld.GetPendingImpactEffectStateList();
		const std::uint32_t serverTick = gameWorld.GetServerTick();

		for (const SnapshotRoomContext& roomContext : roomContextList)
		{
			if (roomContext.peerContextList.empty())
			{
				continue;
			}

			const EndpointKeyList endpointKeyList = BuildEndpointKeyList(roomContext.peerContextList);
			const ImpactEffectDataList impactEffectDataList = BuildRoomImpactEffectDataList(roomContext.roomId, impactEffectStateList);
			if (impactEffectDataList.empty())
			{
				continue;
			}

			const std::size_t maxEffectsPerChunk = common::packet::maxImpactEffectsPerPacket;
			const std::size_t chunkCount = (impactEffectDataList.size() + maxEffectsPerChunk - 1) / maxEffectsPerChunk;
			for (std::size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
			{
				ImpactEffectTask impactEffectTask{};
				impactEffectTask.endpointKeyList = endpointKeyList;
				impactEffectTask.effectPacket.serverTick = serverTick;
				impactEffectTask.effectPacket.roomId = roomContext.roomId;
				impactEffectTask.effectPacket.chunkIndex = static_cast<std::uint16_t>(chunkIndex);
				impactEffectTask.effectPacket.chunkCount = static_cast<std::uint16_t>(chunkCount);

				const std::size_t beginIndex = chunkIndex * maxEffectsPerChunk;
				const std::size_t endIndex = std::min(beginIndex + maxEffectsPerChunk, impactEffectDataList.size());

				for (std::size_t index = beginIndex; index < endIndex; ++index)
				{
					impactEffectTask.effectPacket.effects[impactEffectTask.effectPacket.effectCount] = impactEffectDataList[index];
					++impactEffectTask.effectPacket.effectCount;
				}

				impactEffectTaskList.push_back(std::move(impactEffectTask));
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

	void SnapshotBroadcastBuilder::FillPlayerSnapshotBase(common::packet::PlayerSnapshotPacket& snapshotPacket, RoomId roomId, const SnapshotPeerContextList& peerContextList, const PlayerTable& playerTable, std::uint32_t serverTick) const
	{
		snapshotPacket.serverTick = serverTick;
		snapshotPacket.roomId = roomId;

		for (const SnapshotPeerContext& peerContext : peerContextList)
		{
			const auto playerIterator = playerTable.find(peerContext.playerId);
			if (playerIterator == playerTable.end())
			{
				continue;
			}

			if (snapshotPacket.playerCount >= snapshotPacket.players.size())
			{
				break;
			}

			const game::PlayerState& playerState = playerIterator->second;
			common::packet::PlayerStateData& playerStateData = snapshotPacket.players[snapshotPacket.playerCount];

			playerStateData.playerId = playerState.playerId;
			playerStateData.x = playerState.x;
			playerStateData.y = playerState.y;
			playerStateData.hp = playerState.hp;
			playerStateData.isDead = playerState.isDead ? 1 : 0;
			playerStateData.killCount = playerState.killCount;
			playerStateData.deathCount = playerState.deathCount;
			playerStateData.respawnRemainingSeconds = playerState.respawnRemainingSeconds;
			playerStateData.invincibilityRemainingSeconds = playerState.invincibilityRemainingSeconds;
			playerStateData.hitFlashRemainingSeconds = playerState.hitFlashRemainingSeconds;

			++snapshotPacket.playerCount;
		}
	}

	SnapshotBroadcastBuilder::BulletStateDataList SnapshotBroadcastBuilder::BuildRoomBulletStateDataList(RoomId roomId, const BulletStateList& bulletStateList) const
	{
		BulletStateDataList bulletStateDataList;
		bulletStateDataList.reserve(bulletStateList.size());

		for (const game::BulletState& bulletState : bulletStateList)
		{
			if (bulletState.roomId != roomId)
			{
				continue;
			}

			common::packet::BulletStateData bulletStateData{};
			bulletStateData.bulletId = bulletState.bulletId;
			bulletStateData.x = bulletState.x;
			bulletStateData.y = bulletState.y;

			bulletStateDataList.push_back(bulletStateData);
		}

		return bulletStateDataList;
	}

	SnapshotBroadcastBuilder::ImpactEffectDataList SnapshotBroadcastBuilder::BuildRoomImpactEffectDataList(RoomId roomId, const ImpactEffectStateList& impactEffectStateList) const
	{
		ImpactEffectDataList impactEffectDataList;
		impactEffectDataList.reserve(impactEffectStateList.size());

		for (const game::ImpactEffectState& impactEffectState : impactEffectStateList)
		{
			if (impactEffectState.roomId != roomId)
			{
				continue;
			}

			common::packet::ImpactEffectData impactEffectData{};
			impactEffectData.effectType = impactEffectState.effectType;
			impactEffectData.x = impactEffectState.x;
			impactEffectData.y = impactEffectState.y;

			impactEffectDataList.push_back(impactEffectData);
		}

		return impactEffectDataList;
	}
}