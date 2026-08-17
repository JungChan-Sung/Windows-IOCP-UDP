#include "SnapshotChunkAssemblerTests.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include <Common/Packet/Game/GamePacket.h>

#include <Client/Net/SnapshotChunkAssembler.h>

#include <Tests/DebugTestResult.h>

namespace
{
	[[nodiscard]] common::packet::BulletSnapshotPacket MakeBulletPacket(
		std::int32_t roomId,
		std::uint32_t serverTick,
		std::uint16_t chunkIndex,
		std::uint16_t chunkCount
	)
	{
		common::packet::BulletSnapshotPacket packet{};
		packet.roomId = roomId;
		packet.serverTick = serverTick;
		packet.chunkIndex = chunkIndex;
		packet.chunkCount = chunkCount;
		return packet;
	}

	[[nodiscard]] common::packet::ImpactEffectPacket MakeImpactEffectPacket(
		std::int32_t roomId,
		std::uint32_t serverTick,
		std::uint16_t chunkIndex,
		std::uint16_t chunkCount
	)
	{
		common::packet::ImpactEffectPacket packet{};
		packet.roomId = roomId;
		packet.serverTick = serverTick;
		packet.chunkIndex = chunkIndex;
		packet.chunkCount = chunkCount;
		return packet;
	}

	void AddBullet(
		common::packet::BulletSnapshotPacket& packet,
		std::size_t index,
		std::uint32_t bulletId,
		float x,
		float y
	)
	{
		if (index >= packet.bullets.size())
		{
			return;
		}

		packet.bullets[index].bulletId = bulletId;
		packet.bullets[index].x = x;
		packet.bullets[index].y = y;

		const std::size_t nextCount = index + 1;
		if (nextCount > packet.bulletCount)
		{
			packet.bulletCount = static_cast<std::uint16_t>(nextCount);
		}
	}

	void AddImpactEffect(
		common::packet::ImpactEffectPacket& packet,
		std::size_t index,
		common::game::EffectType effectType,
		float x,
		float y
	)
	{
		if (index >= packet.effects.size())
		{
			return;
		}

		packet.effects[index].effectType = effectType;
		packet.effects[index].x = x;
		packet.effects[index].y = y;

		const std::size_t nextCount = index + 1;
		if (nextCount > packet.effectCount)
		{
			packet.effectCount = static_cast<std::uint16_t>(nextCount);
		}
	}

	void RunSingleBulletChunkTest(tests::DebugTestResult& result)
	{
		client::net::SnapshotChunkAssembler assembler;

		common::packet::BulletSnapshotPacket packet = MakeBulletPacket(1, 100, 0, 1);
		AddBullet(packet, 0, 10, 100.0F, 200.0F);
		AddBullet(packet, 1, 11, 300.0F, 400.0F);

		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> assembledSnapshot
			= assembler.PushBulletSnapshotChunk(packet);

		tests::Expect(result, assembledSnapshot.has_value(), "ClientSnapshotAssembler: single bullet chunk assembled");

		if (!assembledSnapshot.has_value())
		{
			return;
		}

		tests::Expect(result, assembledSnapshot->roomId == 1, "ClientSnapshotAssembler: single bullet roomId");
		tests::Expect(result, assembledSnapshot->serverTick == 100, "ClientSnapshotAssembler: single bullet serverTick");
		tests::Expect(result, assembledSnapshot->bulletStateDataList.size() == 2, "ClientSnapshotAssembler: single bullet count");
		tests::Expect(result, assembledSnapshot->bulletStateDataList[0].bulletId == 10, "ClientSnapshotAssembler: single bullet data 0");
		tests::Expect(result, assembledSnapshot->bulletStateDataList[1].bulletId == 11, "ClientSnapshotAssembler: single bullet data 1");
	}

	void RunMultiBulletChunkInOrderTest(tests::DebugTestResult& result)
	{
		client::net::SnapshotChunkAssembler assembler;

		common::packet::BulletSnapshotPacket firstPacket = MakeBulletPacket(1, 200, 0, 2);
		AddBullet(firstPacket, 0, 20, 10.0F, 20.0F);

		common::packet::BulletSnapshotPacket secondPacket = MakeBulletPacket(1, 200, 1, 2);
		AddBullet(secondPacket, 0, 21, 30.0F, 40.0F);
		AddBullet(secondPacket, 1, 22, 50.0F, 60.0F);

		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> firstResult
			= assembler.PushBulletSnapshotChunk(firstPacket);
		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> secondResult
			= assembler.PushBulletSnapshotChunk(secondPacket);

		tests::Expect(result, !firstResult.has_value(), "ClientSnapshotAssembler: multi bullet first waits");
		tests::Expect(result, secondResult.has_value(), "ClientSnapshotAssembler: multi bullet second assembles");

		if (!secondResult.has_value())
		{
			return;
		}

		tests::Expect(result, secondResult->bulletStateDataList.size() == 3, "ClientSnapshotAssembler: multi bullet count");
		tests::Expect(result, secondResult->bulletStateDataList[0].bulletId == 20, "ClientSnapshotAssembler: multi bullet order 0");
		tests::Expect(result, secondResult->bulletStateDataList[1].bulletId == 21, "ClientSnapshotAssembler: multi bullet order 1");
		tests::Expect(result, secondResult->bulletStateDataList[2].bulletId == 22, "ClientSnapshotAssembler: multi bullet order 2");
	}

	void RunMultiBulletChunkOutOfOrderTest(tests::DebugTestResult& result)
	{
		client::net::SnapshotChunkAssembler assembler;

		common::packet::BulletSnapshotPacket firstPacket = MakeBulletPacket(1, 300, 1, 2);
		AddBullet(firstPacket, 0, 31, 30.0F, 40.0F);

		common::packet::BulletSnapshotPacket secondPacket = MakeBulletPacket(1, 300, 0, 2);
		AddBullet(secondPacket, 0, 30, 10.0F, 20.0F);

		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> firstResult
			= assembler.PushBulletSnapshotChunk(firstPacket);
		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> secondResult
			= assembler.PushBulletSnapshotChunk(secondPacket);

		tests::Expect(result, !firstResult.has_value(), "ClientSnapshotAssembler: out-of-order bullet first waits");
		tests::Expect(result, secondResult.has_value(), "ClientSnapshotAssembler: out-of-order bullet second assembles");

		if (!secondResult.has_value())
		{
			return;
		}

		tests::Expect(result, secondResult->bulletStateDataList.size() == 2, "ClientSnapshotAssembler: out-of-order bullet count");
		tests::Expect(result, secondResult->bulletStateDataList[0].bulletId == 30, "ClientSnapshotAssembler: out-of-order bullet order 0");
		tests::Expect(result, secondResult->bulletStateDataList[1].bulletId == 31, "ClientSnapshotAssembler: out-of-order bullet order 1");
	}

	void RunDuplicateBulletChunkTest(tests::DebugTestResult& result)
	{
		client::net::SnapshotChunkAssembler assembler;

		common::packet::BulletSnapshotPacket packet = MakeBulletPacket(1, 400, 0, 2);
		AddBullet(packet, 0, 40, 10.0F, 20.0F);

		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> firstResult
			= assembler.PushBulletSnapshotChunk(packet);
		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> duplicateResult
			= assembler.PushBulletSnapshotChunk(packet);

		tests::Expect(result, !firstResult.has_value(), "ClientSnapshotAssembler: duplicate bullet first waits");
		tests::Expect(result, !duplicateResult.has_value(), "ClientSnapshotAssembler: duplicate bullet dropped");
	}

	void RunAlreadyAppliedBulletChunkTest(tests::DebugTestResult& result)
	{
		client::net::SnapshotChunkAssembler assembler;

		common::packet::BulletSnapshotPacket packet = MakeBulletPacket(1, 500, 0, 1);
		AddBullet(packet, 0, 50, 10.0F, 20.0F);

		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> firstResult
			= assembler.PushBulletSnapshotChunk(packet);
		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> secondResult
			= assembler.PushBulletSnapshotChunk(packet);

		tests::Expect(result, firstResult.has_value(), "ClientSnapshotAssembler: already-applied bullet first assembles");
		tests::Expect(result, !secondResult.has_value(), "ClientSnapshotAssembler: already-applied bullet dropped");
	}

	void RunResetRoomBulletTest(tests::DebugTestResult& result)
	{
		client::net::SnapshotChunkAssembler assembler;

		common::packet::BulletSnapshotPacket firstPacket = MakeBulletPacket(1, 600, 0, 2);
		AddBullet(firstPacket, 0, 60, 10.0F, 20.0F);

		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> firstResult
			= assembler.PushBulletSnapshotChunk(firstPacket);

		assembler.ResetRoom(1);

		common::packet::BulletSnapshotPacket secondPacket = MakeBulletPacket(1, 600, 1, 2);
		AddBullet(secondPacket, 0, 61, 30.0F, 40.0F);

		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> secondResult
			= assembler.PushBulletSnapshotChunk(secondPacket);

		tests::Expect(result, !firstResult.has_value(), "ClientSnapshotAssembler: reset room bullet first waits");
		tests::Expect(result, !secondResult.has_value(), "ClientSnapshotAssembler: reset room removes partial assembly");
	}

	void RunSingleImpactEffectChunkTest(tests::DebugTestResult& result)
	{
		client::net::SnapshotChunkAssembler assembler;

		common::packet::ImpactEffectPacket packet = MakeImpactEffectPacket(2, 700, 0, 1);
		AddImpactEffect(packet, 0, common::game::EffectType::Impact, 100.0F, 200.0F);
		AddImpactEffect(packet, 1, common::game::EffectType::Spawn, 300.0F, 400.0F);

		const std::optional<client::net::SnapshotChunkAssembler::AssembledImpactEffectPacket> assembledPacket
			= assembler.PushImpactEffectChunk(packet);

		tests::Expect(result, assembledPacket.has_value(), "ClientSnapshotAssembler: single impact chunk assembled");

		if (!assembledPacket.has_value())
		{
			return;
		}

		tests::Expect(result, assembledPacket->roomId == 2, "ClientSnapshotAssembler: single impact roomId");
		tests::Expect(result, assembledPacket->serverTick == 700, "ClientSnapshotAssembler: single impact serverTick");
		tests::Expect(result, assembledPacket->impactEffectDataList.size() == 2, "ClientSnapshotAssembler: single impact count");
		tests::Expect(result, assembledPacket->impactEffectDataList[0].effectType == common::game::EffectType::Impact,
			"ClientSnapshotAssembler: single impact data 0");
		tests::Expect(result, assembledPacket->impactEffectDataList[1].effectType == common::game::EffectType::Spawn,
			"ClientSnapshotAssembler: single impact data 1");
	}

	void RunMultiImpactEffectChunkOutOfOrderTest(tests::DebugTestResult& result)
	{
		client::net::SnapshotChunkAssembler assembler;

		common::packet::ImpactEffectPacket firstPacket = MakeImpactEffectPacket(2, 800, 1, 2);
		AddImpactEffect(firstPacket, 0, common::game::EffectType::Spawn, 30.0F, 40.0F);

		common::packet::ImpactEffectPacket secondPacket = MakeImpactEffectPacket(2, 800, 0, 2);
		AddImpactEffect(secondPacket, 0, common::game::EffectType::Impact, 10.0F, 20.0F);

		const std::optional<client::net::SnapshotChunkAssembler::AssembledImpactEffectPacket> firstResult
			= assembler.PushImpactEffectChunk(firstPacket);
		const std::optional<client::net::SnapshotChunkAssembler::AssembledImpactEffectPacket> secondResult
			= assembler.PushImpactEffectChunk(secondPacket);

		tests::Expect(result, !firstResult.has_value(), "ClientSnapshotAssembler: out-of-order impact first waits");
		tests::Expect(result, secondResult.has_value(), "ClientSnapshotAssembler: out-of-order impact second assembles");

		if (!secondResult.has_value())
		{
			return;
		}

		tests::Expect(result, secondResult->impactEffectDataList.size() == 2, "ClientSnapshotAssembler: out-of-order impact count");
		tests::Expect(result, secondResult->impactEffectDataList[0].effectType == common::game::EffectType::Impact,
			"ClientSnapshotAssembler: out-of-order impact order 0");
		tests::Expect(result, secondResult->impactEffectDataList[1].effectType == common::game::EffectType::Spawn,
			"ClientSnapshotAssembler: out-of-order impact order 1");
	}

	void RunBulletAndImpactIndependentTest(tests::DebugTestResult& result)
	{
		client::net::SnapshotChunkAssembler assembler;

		common::packet::BulletSnapshotPacket bulletPacket = MakeBulletPacket(1, 900, 0, 1);
		AddBullet(bulletPacket, 0, 90, 10.0F, 20.0F);

		common::packet::ImpactEffectPacket impactPacket = MakeImpactEffectPacket(1, 900, 0, 1);
		AddImpactEffect(impactPacket, 0, common::game::EffectType::Impact, 10.0F, 20.0F);

		const std::optional<client::net::SnapshotChunkAssembler::AssembledBulletSnapshot> bulletResult
			= assembler.PushBulletSnapshotChunk(bulletPacket);
		const std::optional<client::net::SnapshotChunkAssembler::AssembledImpactEffectPacket> impactResult
			= assembler.PushImpactEffectChunk(impactPacket);

		tests::Expect(result, bulletResult.has_value(), "ClientSnapshotAssembler: bullet stream assembles independently");
		tests::Expect(result, impactResult.has_value(), "ClientSnapshotAssembler: impact stream assembles independently");

		if (bulletResult.has_value())
		{
			tests::Expect(result, bulletResult->bulletStateDataList.size() == 1, "ClientSnapshotAssembler: independent bullet count");
		}

		if (impactResult.has_value())
		{
			tests::Expect(result, impactResult->impactEffectDataList.size() == 1, "ClientSnapshotAssembler: independent impact count");
		}
	}
}

namespace tests::client
{
	tests::DebugTestResult RunSnapshotChunkAssemblerTests()
	{
		tests::DebugTestResult result{};

		RunSingleBulletChunkTest(result);
		RunMultiBulletChunkInOrderTest(result);
		RunMultiBulletChunkOutOfOrderTest(result);
		RunDuplicateBulletChunkTest(result);
		RunAlreadyAppliedBulletChunkTest(result);
		RunResetRoomBulletTest(result);
		RunSingleImpactEffectChunkTest(result);
		RunMultiImpactEffectChunkOutOfOrderTest(result);
		RunBulletAndImpactIndependentTest(result);

		return result;
	}
}

