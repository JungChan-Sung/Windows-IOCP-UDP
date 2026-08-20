#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Net/EndpointKey.h>
#include <Common/Packet/Game/GamePacket.h>

#include <Server/Game/GameWorld.h>
#include <Server/Protocol/SnapshotBroadcastContext.h>
#include <Server/Protocol/SnapshotBroadcastTask.h>

namespace server::protocol
{
	class SnapshotBroadcastBuilder
	{
	public:
		using BulletStateDataList = std::vector<common::packet::BulletStateData>;
		using ImpactEffectDataList = std::vector<common::packet::ImpactEffectData>;

		using RoomId = common::game::RoomId;

		using PlayerTable = game::GameWorld::PlayerTable;
		using BulletStateList = game::GameWorld::BulletStateList;
		using ImpactEffectStateList = game::GameWorld::ImpactEffectStateList;

	public:
		SnapshotBroadcastBuilder() = default;
		~SnapshotBroadcastBuilder() noexcept = default;
		
		SnapshotBroadcastBuilder(const SnapshotBroadcastBuilder&) = delete;
		SnapshotBroadcastBuilder& operator=(const SnapshotBroadcastBuilder&) = delete;

		SnapshotBroadcastBuilder(SnapshotBroadcastBuilder&&) = delete;
		SnapshotBroadcastBuilder& operator=(SnapshotBroadcastBuilder&&) = delete;

	public:
		[[nodiscard]] std::vector<PlayerSnapshotTask> BuildPlayerSnapshotTasks(
			std::span<const SnapshotRoomContext> roomContextList,
			const game::GameWorld& gameWorld
		) const;
		[[nodiscard]] std::vector<BulletSnapshotTask> BuildBulletSnapshotTasks(
			std::span<const SnapshotRoomContext> roomContextList,
			const game::GameWorld& gameWorld
		) const;
		[[nodiscard]] std::vector<ImpactEffectTask> BuildImpactEffectTasks(
			std::span<const SnapshotRoomContext> roomContextList,
			const game::GameWorld& gameWorld
		) const;

	private:
		[[nodiscard]] EndpointKeyList BuildEndpointKeyList(const SnapshotPeerContextList& peerContextList) const;

		void FillPlayerSnapshotBase(
			common::packet::PlayerSnapshotPacket& snapshotPacket,
			RoomId roomId,
			const SnapshotPeerContextList& peerContextList,
			const PlayerTable& playerTable,
			std::uint32_t serverTick
		) const;

		[[nodiscard]] BulletStateDataList BuildRoomBulletStateDataList(RoomId roomId, const BulletStateList& bulletStateList) const;
		[[nodiscard]] ImpactEffectDataList BuildRoomImpactEffectDataList(RoomId roomId, const ImpactEffectStateList& impactEffectStateList) const;
	};
}