#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Net/EndpointKey.h>
#include <Common/Packet/Game/GamePacket.h>

#include <Server/Game/GameWorld.h>
#include <Server/Protocol/SnapshotBroadcastTask.h>
#include <Server/Service/PeerRoomManager.h>
#include <Server/Service/PeerState.h>

namespace server::protocol
{
	class SnapshotBroadcastBuilder
	{
	public:
		using BulletStateDataList = std::vector<common::packet::BulletStateData>;
		using ImpactEffectDataList = std::vector<common::packet::ImpactEffectData>;

		using RoomId = common::game::RoomId;
		using EndpointKey = common::net::EndpointKey;

		using PeerTable = service::PeerRoomManager::PeerTable;
		using RoomMemberSet = service::PeerRoomManager::RoomMemberSet;
		using RoomTable = service::PeerRoomManager::RoomTable;

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
			const RoomTable& roomTable,
			const PeerTable& peerTable,
			const game::GameWorld& gameWorld
		) const;
		[[nodiscard]] std::vector<BulletSnapshotTask> BuildBulletSnapshotTasks(
			const RoomTable& roomTable,
			const PeerTable& peerTable,
			const game::GameWorld& gameWorld
		) const;
		[[nodiscard]] std::vector<ImpactEffectTask> BuildImpactEffectTasks(
			const RoomTable& roomTable,
			const PeerTable& peerTable,
			const game::GameWorld& gameWorld
		) const;

	private:
		[[nodiscard]] EndpointKeyList BuildRoomEndpointKeyList(const RoomMemberSet& roomMemberSet, const PeerTable& peerTable) const;

		void FillPlayerSnapshotBase(
			common::packet::PlayerSnapshotPacket& snapshotPacket,
			RoomId roomId,
			const RoomMemberSet& roomMemberSet,
			const PeerTable& peerTable,
			const PlayerTable& playerTable,
			std::uint32_t serverTick
		) const;

		[[nodiscard]] BulletStateDataList BuildRoomBulletStateDataList(
			RoomId roomId,
			const BulletStateList& bulletStateList
		) const;
		[[nodiscard]] ImpactEffectDataList BuildRoomImpactEffectDataList(
			RoomId roomId,
			const ImpactEffectStateList& impactEffectStateList
		) const;
	};
}