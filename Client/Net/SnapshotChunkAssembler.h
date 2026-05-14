#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Net/SnapshotChunkAssemblerCore.h>
#include <Common/Packet/GamePacket.h>

#include <Client/Config/ClientConfigDefaults.h>

namespace client::net
{
	class SnapshotChunkAssembler
	{
	public:
		using RoomId = common::game::RoomId;

	public:
		struct AssembledBulletSnapshot
		{
		public:
			std::uint32_t serverTick = 0;
			RoomId roomId = 0;
			std::vector<common::packet::BulletStateData> bulletStateDataList;
		};

		struct AssembledImpactEffectPacket
		{
		public:
			std::uint32_t serverTick = 0;
			RoomId roomId = 0;
			std::vector<common::packet::ImpactEffectData> impactEffectDataList;
		};

	private:
		using BulletAssemblerCore = common::net::SnapshotChunkAssemblerCore<common::packet::BulletStateData, RoomId>;
		using ImpactEffectAssemblerCore = common::net::SnapshotChunkAssemblerCore<common::packet::ImpactEffectData, RoomId>;

	private:
		BulletAssemblerCore bulletAssemblerCore_;
		ImpactEffectAssemblerCore impactEffectAssemblerCore_;
		std::chrono::milliseconds assemblyTimeout_ = config::defaultSnapshotAssemblyTimeout;

	public:
		SnapshotChunkAssembler() = default;
		~SnapshotChunkAssembler() noexcept = default;

		SnapshotChunkAssembler(const SnapshotChunkAssembler&) = delete;
		SnapshotChunkAssembler& operator=(const SnapshotChunkAssembler&) = delete;

		SnapshotChunkAssembler(SnapshotChunkAssembler&&) = delete;
		SnapshotChunkAssembler& operator=(SnapshotChunkAssembler&&) = delete;

	public:
		void Clear() noexcept;
		void ResetRoom(RoomId roomId) noexcept;

		[[nodiscard]] std::optional<AssembledBulletSnapshot> PushBulletSnapshotChunk(
			const common::packet::BulletSnapshotPacket& packet
		);
		[[nodiscard]] std::optional<AssembledImpactEffectPacket> PushImpactEffectChunk(
			const common::packet::ImpactEffectPacket& packet
		);

	public:
		void SetAssemblyTimeout(std::chrono::milliseconds assemblyTimeout) noexcept;

		[[nodiscard]] std::chrono::milliseconds GetAssemblyTimeout() const noexcept
		{
			return assemblyTimeout_;
		}
	};
}