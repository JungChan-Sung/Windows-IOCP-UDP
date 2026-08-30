#pragma once

#include <vector>

#include <Common/Packet/Game/GamePacket.h>

#include <Server/Protocol/SnapshotBroadcastContext.h>
#include <Server/Protocol/SnapshotBroadcastTask.h>

namespace server::protocol
{
	class SnapshotBroadcastBuilder
	{
	public:
		SnapshotBroadcastBuilder() = default;
		~SnapshotBroadcastBuilder() noexcept = default;
		
		SnapshotBroadcastBuilder(const SnapshotBroadcastBuilder&) = delete;
		SnapshotBroadcastBuilder& operator=(const SnapshotBroadcastBuilder&) = delete;

		SnapshotBroadcastBuilder(SnapshotBroadcastBuilder&&) = delete;
		SnapshotBroadcastBuilder& operator=(SnapshotBroadcastBuilder&&) = delete;

	public:
		[[nodiscard]] std::vector<PlayerSnapshotTask> BuildPlayerSnapshotTasks(const SnapshotBroadcastContext& context) const;
		[[nodiscard]] std::vector<BulletSnapshotTask> BuildBulletSnapshotTasks(const SnapshotBroadcastContext& context) const;
		[[nodiscard]] std::vector<ImpactEffectTask> BuildImpactEffectTasks(const SnapshotBroadcastContext& context) const;

	private:
		[[nodiscard]] EndpointKeyList BuildEndpointKeyList(const SnapshotPeerContextList& peerContextList) const;

		void FillPlayerSnapshotBase(
			common::packet::PlayerSnapshotPacket& snapshotPacket,
			const SnapshotRoomContext& roomContext,
			const SnapshotBroadcastContext& context
		) const;
	};
}