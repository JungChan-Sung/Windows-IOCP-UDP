#include "SnapshotChunkAssembler.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <type_traits>
#include <utility>
#include <concepts>

#include <Common/Time/TimeTypes.h>

#include <Client/Config/ClientConfigDefaults.h>

namespace
{
	using BulletChunkView = common::net::SnapshotChunkView<common::packet::BulletStateData, common::game::RoomId>;
	using ImpactEffectChunkView = common::net::SnapshotChunkView<common::packet::ImpactEffectData, common::game::RoomId>;

#if defined(_DEBUG)
	void DebugLogBulletChunk(
		const char* tag,
		std::int32_t roomId,
		std::uint32_t serverTick,
		std::uint16_t chunkIndex,
		std::uint16_t chunkCount,
		std::uint16_t receivedChunkCount,
		std::uint32_t lastAppliedTick,
		std::size_t payloadCount
	) noexcept
	{
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"[BulletChunk] %s roomId=%d serverTick=%u chunkIndex=%hu chunkCount=%hu receivedChunkCount=%hu lastAppliedTick=%u payloadCount=%zu\n",
			tag,
			roomId,
			serverTick,
			chunkIndex,
			chunkCount,
			receivedChunkCount,
			lastAppliedTick,
			payloadCount
		);
		::OutputDebugStringA(buffer);
	}

	void DebugLogImpactChunk(
		const char* tag,
		std::int32_t roomId,
		std::uint32_t serverTick,
		std::uint16_t chunkIndex,
		std::uint16_t chunkCount,
		std::uint16_t receivedChunkCount,
		std::uint32_t lastAppliedTick,
		std::size_t payloadCount
	) noexcept
	{
		char buffer[256]{};
		std::snprintf(
			buffer,
			sizeof(buffer),
			"[ImpactChunk] %s roomId=%d serverTick=%u chunkIndex=%hu chunkCount=%hu receivedChunkCount=%hu lastAppliedTick=%u payloadCount=%zu\n",
			tag,
			roomId,
			serverTick,
			chunkIndex,
			chunkCount,
			receivedChunkCount,
			lastAppliedTick,
			payloadCount
		);
		::OutputDebugStringA(buffer);
	}
#else
	void DebugLogBulletChunk(
		const char*,
		std::int32_t,
		std::uint32_t,
		std::uint16_t,
		std::uint16_t,
		std::uint16_t,
		std::uint32_t,
		std::size_t
	) noexcept
	{}

	void DebugLogImpactChunk(
		const char*,
		std::int32_t,
		std::uint32_t,
		std::uint16_t,
		std::uint16_t,
		std::uint16_t,
		std::uint32_t,
		std::size_t
	) noexcept
	{}
#endif

	[[nodiscard]] BulletChunkView MakeBulletChunkView(const common::packet::BulletSnapshotPacket& packet)
	{
		BulletChunkView chunkView{};
		chunkView.roomId = packet.roomId;
		chunkView.serverTick = packet.serverTick;
		chunkView.chunkIndex = packet.chunkIndex;
		chunkView.chunkCount = packet.chunkCount;

		const std::size_t bulletCount = std::min(static_cast<std::size_t>(packet.bulletCount), packet.bullets.size());

		chunkView.dataList.reserve(bulletCount);
		for (std::size_t index = 0; index < bulletCount; ++index)
		{
			chunkView.dataList.push_back(packet.bullets[index]);
		}

		return chunkView;
	}

	[[nodiscard]] ImpactEffectChunkView MakeImpactEffectChunkView(const common::packet::ImpactEffectPacket& packet)
	{
		ImpactEffectChunkView chunkView{};
		chunkView.roomId = packet.roomId;
		chunkView.serverTick = packet.serverTick;
		chunkView.chunkIndex = packet.chunkIndex;
		chunkView.chunkCount = packet.chunkCount;

		const std::size_t effectCount = std::min(static_cast<std::size_t>(packet.effectCount), packet.effects.size());

		chunkView.dataList.reserve(effectCount);
		for (std::size_t index = 0; index < effectCount; ++index)
		{
			chunkView.dataList.push_back(packet.effects[index]);
		}

		return chunkView;
	}

	[[nodiscard]] client::net::SnapshotChunkAssembler::AssembledBulletSnapshot ToAssembledBulletSnapshot(
		common::net::AssembledSnapshotChunk<common::packet::BulletStateData, common::game::RoomId>&& assembledChunk
	)
	{
		client::net::SnapshotChunkAssembler::AssembledBulletSnapshot assembledBulletSnapshot{};
		assembledBulletSnapshot.roomId = assembledChunk.roomId;
		assembledBulletSnapshot.serverTick = assembledChunk.serverTick;
		assembledBulletSnapshot.bulletStateDataList = std::move(assembledChunk.dataList);

		return assembledBulletSnapshot;
	}

	[[nodiscard]] client::net::SnapshotChunkAssembler::AssembledImpactEffectPacket ToAssembledImpactEffectPacket(
		common::net::AssembledSnapshotChunk<common::packet::ImpactEffectData, common::game::RoomId>&& assembledChunk
	)
	{
		client::net::SnapshotChunkAssembler::AssembledImpactEffectPacket assembledImpactEffectPacket{};
		assembledImpactEffectPacket.roomId = assembledChunk.roomId;
		assembledImpactEffectPacket.serverTick = assembledChunk.serverTick;
		assembledImpactEffectPacket.impactEffectDataList = std::move(assembledChunk.dataList);

		return assembledImpactEffectPacket;
	}
}

namespace client::net
{
	void SnapshotChunkAssembler::Clear() noexcept
	{
		bulletAssemblerCore_.Clear();
		impactEffectAssemblerCore_.Clear();
	}

	void SnapshotChunkAssembler::ResetRoom(RoomId roomId) noexcept
	{
		bulletAssemblerCore_.ResetRoom(roomId);
		impactEffectAssemblerCore_.ResetRoom(roomId);
	}

	std::optional<SnapshotChunkAssembler::AssembledBulletSnapshot> SnapshotChunkAssembler::PushBulletSnapshotChunk(const common::packet::BulletSnapshotPacket& packet)
	{
		const auto currentTime = std::chrono::steady_clock::now();

		bulletAssemblerCore_.CleanupExpiredAssemblies(
			currentTime,
			assemblyTimeout_,
			DebugLogBulletChunk
		);

		std::optional<common::net::AssembledSnapshotChunk<common::packet::BulletStateData, RoomId>> assembledChunk
			= bulletAssemblerCore_.PushChunk(
				MakeBulletChunkView(packet),
				currentTime,
				DebugLogBulletChunk
			);

		if (!assembledChunk.has_value())
		{
			return std::nullopt;
		}

		return ToAssembledBulletSnapshot(std::move(*assembledChunk));
	}

	std::optional<SnapshotChunkAssembler::AssembledImpactEffectPacket> SnapshotChunkAssembler::PushImpactEffectChunk(const common::packet::ImpactEffectPacket& packet)
	{
		const auto currentTime = std::chrono::steady_clock::now();

		impactEffectAssemblerCore_.CleanupExpiredAssemblies(
			currentTime,
			assemblyTimeout_,
			DebugLogImpactChunk
		);

		std::optional<common::net::AssembledSnapshotChunk<common::packet::ImpactEffectData, RoomId>> assembledChunk
			= impactEffectAssemblerCore_.PushChunk(
				MakeImpactEffectChunkView(packet),
				currentTime,
				DebugLogImpactChunk
			);

		if (!assembledChunk.has_value())
		{
			return std::nullopt;
		}

		return ToAssembledImpactEffectPacket(std::move(*assembledChunk));
	}

	void SnapshotChunkAssembler::SetAssemblyTimeout(common::time::Milliseconds assemblyTimeout) noexcept
	{
		if (assemblyTimeout <= common::time::Milliseconds(0))
		{
			assemblyTimeout_ = config::defaultSnapshotAssemblyTimeout;
			return;
		}

		assemblyTimeout_ = assemblyTimeout;
	}

}