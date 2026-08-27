#include "ClientWorldTests.h"

#include <cmath>

#include <Common/Game/EffectType.h>
#include <Common/Game/InputFlags.h>
#include <Common/Game/SimulationConstants.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Time/TimeTypes.h>

#include <Client/Game/ClientWorld.h>

#include <Tests/DebugTestResult.h>

namespace
{
	inline constexpr float floatTolerance = 0.001F;

	[[nodiscard]] bool IsNearlyEqual(float left, float right) noexcept
	{
		return std::abs(left - right) <= floatTolerance;
	}

	void RunValidJoinStateAcceptedTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		const bool setResult = world.TrySetJoinState(100, 2, 120.0F, 240.0F);

		tests::Expect(result, setResult, "ClientWorld: valid join state accepted");
		tests::Expect(result, world.IsJoined(), "ClientWorld: joined after valid join state");
		tests::Expect(result, world.GetLocalPlayerId() == 100, "ClientWorld: local player id set");
		tests::Expect(result, world.GetCurrentRoomId() == 2, "ClientWorld: room id set");
	}

	void RunZeroPlayerIdRejectedTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		const bool setResult = world.TrySetJoinState(0, 1, 120.0F, 240.0F);

		tests::Expect(result, !setResult, "ClientWorld: zero player id rejected");
		tests::Expect(result, !world.IsJoined(), "ClientWorld: zero player id does not join");
		tests::Expect(result, world.GetLocalPlayerId() == 0, "ClientWorld: zero player id leaves local player unset");
		tests::Expect(result, world.GetCurrentRoomId() == 0, "ClientWorld: zero player id leaves room unset");
	}

	void RunInvalidRoomIdRejectedTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		const bool zeroRoomResult = world.TrySetJoinState(100, 0, 120.0F, 240.0F);
		const bool negativeRoomResult = world.TrySetJoinState(100, -1, 120.0F, 240.0F);

		tests::Expect(result, !zeroRoomResult, "ClientWorld: zero room id rejected");
		tests::Expect(result, !negativeRoomResult, "ClientWorld: negative room id rejected");
		tests::Expect(result, !world.IsJoined(), "ClientWorld: invalid room id does not join");
		tests::Expect(result, world.GetLocalPlayerId() == 0, "ClientWorld: invalid room leaves local player unset");
		tests::Expect(result, world.GetCurrentRoomId() == 0, "ClientWorld: invalid room leaves room unset");
	}

	void RunDuplicateJoinStateRejectedTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		const bool firstResult = world.TrySetJoinState(100, 2, 120.0F, 240.0F);
		const bool duplicateResult = world.TrySetJoinState(200, 3, 500.0F, 600.0F);

		tests::Expect(result, firstResult, "ClientWorld: first join state accepted");
		tests::Expect(result, !duplicateResult, "ClientWorld: duplicate join state rejected");
		tests::Expect(result, world.GetLocalPlayerId() == 100, "ClientWorld: duplicate join preserves player id");
		tests::Expect(result, world.GetCurrentRoomId() == 2, "ClientWorld: duplicate join preserves room id");
	}

	void RunClearAllowsNewJoinStateTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		const bool firstResult = world.TrySetJoinState(100, 1, 120.0F, 240.0F);

		world.Clear();

		const bool secondResult = world.TrySetJoinState(200, 3, 500.0F, 600.0F);

		tests::Expect(result, firstResult, "ClientWorld: first join before clear accepted");
		tests::Expect(result, secondResult, "ClientWorld: join after clear accepted");
		tests::Expect(result, world.IsJoined(), "ClientWorld: joined after clear and rejoin");
		tests::Expect(result, world.GetLocalPlayerId() == 200, "ClientWorld: rejoin player id set");
		tests::Expect(result, world.GetCurrentRoomId() == 3, "ClientWorld: rejoin room id set");
	}

	void RunLocalPredictionAppliedToRenderFrameTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		const bool joinResult = world.TrySetJoinState(
			100,
			1,
			320.0F,
			350.0F
		);

		client::game::ClientWorld::PlayerJoinedEvent playerJoinedEvent{};
		playerJoinedEvent.playerId = 100;
		playerJoinedEvent.x = 320.0F;
		playerJoinedEvent.y = 350.0F;

		world.ApplyPlayerJoinedEvent(playerJoinedEvent);

		world.ApplyLocalPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds
		);

		const client::game::ClientWorld::RenderFrameSnapshot snapshot =
			world.BuildRenderFrameSnapshot(common::time::Clock::now());

		tests::Expect(result, joinResult, "ClientWorld: prediction integration join accepted");
		tests::Expect(result, snapshot.playerStateList.size() == 1, "ClientWorld: prediction integration player count");

		if (snapshot.playerStateList.size() != 1)
		{
			return;
		}

		const client::game::ClientWorld::RenderPlayerState& playerState = snapshot.playerStateList.front();

		const float expectedX =
			320.0F + (common::game::defaultMoveSpeed * common::game::defaultFixedDeltaSeconds);

		tests::Expect(result, playerState.isLocalPlayer, "ClientWorld: predicted player is local");
		tests::Expect(result, IsNearlyEqual(playerState.x, expectedX), "ClientWorld: local prediction immediately updates render x");
		tests::Expect(result, IsNearlyEqual(playerState.y, 350.0F), "ClientWorld: local prediction preserves render y");
	}

	void RunLocalPredictionRequiresJoinTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		client::game::ClientWorld::PlayerJoinedEvent playerJoinedEvent{};
		playerJoinedEvent.playerId = 100;
		playerJoinedEvent.x = 320.0F;
		playerJoinedEvent.y = 350.0F;

		world.ApplyPlayerJoinedEvent(playerJoinedEvent);

		world.ApplyLocalPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds
		);

		const client::game::ClientWorld::RenderFrameSnapshot snapshot =
			world.BuildRenderFrameSnapshot(common::time::Clock::now());

		tests::Expect(result, !world.IsJoined(), "ClientWorld: prediction guard world is not joined");
		tests::Expect(result, snapshot.playerStateList.size() == 1, "ClientWorld: prediction guard player count");

		if (snapshot.playerStateList.size() != 1)
		{
			return;
		}

		const client::game::ClientWorld::RenderPlayerState& playerState = snapshot.playerStateList.front();

		tests::Expect(result, !playerState.isLocalPlayer, "ClientWorld: unjoined player is not local");
		tests::Expect(result, IsNearlyEqual(playerState.x, 320.0F), "ClientWorld: prediction ignored before join x");
		tests::Expect(result, IsNearlyEqual(playerState.y, 350.0F), "ClientWorld: prediction ignored before join y");
	}

	void RunRenderFrameMetadataTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		world.SetInterpolationSettings(
			common::time::Milliseconds(120),
			common::time::Milliseconds(50),
			common::time::Milliseconds(250)
		);

		const bool joinResult = world.TrySetJoinState(100, 2, 120.0F, 240.0F);

		common::packet::PlayerSnapshotPacket playerSnapshot{};
		playerSnapshot.serverTick = 77;
		playerSnapshot.roomId = 2;
		playerSnapshot.playerCount = 1;

		playerSnapshot.players[0].playerId = 100;
		playerSnapshot.players[0].x = 120.0F;
		playerSnapshot.players[0].y = 240.0F;
		playerSnapshot.players[0].hp = 80;
		playerSnapshot.players[0].killCount = 3;
		playerSnapshot.players[0].deathCount = 1;

		world.ApplyPlayerSnapshot(playerSnapshot);

		const client::game::ClientWorld::RenderFrameSnapshot snapshot =
			world.BuildRenderFrameSnapshot(common::time::Clock::now());

		tests::Expect(result, joinResult, "ClientWorld: render frame test join accepted");
		tests::Expect(result, snapshot.localPlayerId == 100, "ClientWorld: render frame local player id");
		tests::Expect(result, snapshot.currentRoomId == 2, "ClientWorld: render frame room id");
		tests::Expect(result, snapshot.lastServerTick == 77, "ClientWorld: render frame server tick");

		tests::Expect(
			result,
			snapshot.interpolationDelay == common::time::Milliseconds(120),
			"ClientWorld: render frame interpolation delay"
		);
	}

	void RunRenderFramePlayerStateTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		const bool joinResult = world.TrySetJoinState(100, 2, 120.0F, 240.0F);

		common::packet::PlayerSnapshotPacket playerSnapshot{};
		playerSnapshot.serverTick = 10;
		playerSnapshot.roomId = 2;
		playerSnapshot.playerCount = 1;

		common::packet::PlayerStateData& playerState = playerSnapshot.players[0];
		playerState.playerId = 100;
		playerState.x = 120.0F;
		playerState.y = 240.0F;
		playerState.hp = 75;
		playerState.isDead = 0;
		playerState.respawnRemainingSeconds = 0.0F;
		playerState.invincibilityRemainingSeconds = 1.5F;
		playerState.hitFlashRemainingSeconds = 0.25F;
		playerState.killCount = 4;
		playerState.deathCount = 2;

		world.ApplyPlayerSnapshot(playerSnapshot);

		const client::game::ClientWorld::RenderFrameSnapshot snapshot =
			world.BuildRenderFrameSnapshot(common::time::Clock::now());

		tests::Expect(result, joinResult, "ClientWorld: render player test join accepted");
		tests::Expect(result, snapshot.playerStateList.size() == 1, "ClientWorld: render frame player count");

		if (snapshot.playerStateList.size() != 1)
		{
			return;
		}

		const client::game::ClientWorld::RenderPlayerState& renderPlayerState = snapshot.playerStateList.front();

		tests::Expect(result, renderPlayerState.playerId == 100, "ClientWorld: render player id");
		tests::Expect(result, renderPlayerState.x == 120.0F, "ClientWorld: render player x");
		tests::Expect(result, renderPlayerState.y == 240.0F, "ClientWorld: render player y");
		tests::Expect(result, renderPlayerState.hp == 75, "ClientWorld: render player hp");
		tests::Expect(result, !renderPlayerState.isDead, "ClientWorld: render player alive state");
		tests::Expect(result, renderPlayerState.invincibilityRemainingSeconds == 1.5F, "ClientWorld: render player invincibility");
		tests::Expect(result, renderPlayerState.hitFlashRemainingSeconds == 0.25F, "ClientWorld: render player hit flash");
		tests::Expect(result, renderPlayerState.killCount == 4, "ClientWorld: render player kill count");
		tests::Expect(result, renderPlayerState.deathCount == 2, "ClientWorld: render player death count");
		tests::Expect(result, renderPlayerState.isLocalPlayer, "ClientWorld: render player local flag");
	}

	void RunRenderFrameBulletAndEffectTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		const bool joinResult = world.TrySetJoinState(100, 2, 120.0F, 240.0F);

		client::game::ClientWorld::BulletStateDataList bulletStateDataList;
		bulletStateDataList.push_back(common::packet::BulletStateData{
			.bulletId = 300,
			.x = 400.0F,
			.y = 500.0F,
			});

		world.ApplyBulletSnapshotData(10, 2, bulletStateDataList);

		client::game::ClientWorld::ImpactEffectDataList impactEffectDataList;
		impactEffectDataList.push_back(common::packet::ImpactEffectData{
			.effectType = common::game::EffectType::Impact,
			.x = 410.0F,
			.y = 510.0F,
			});

		world.ApplyImpactEffectData(10, 2, impactEffectDataList);

		const client::game::ClientWorld::RenderFrameSnapshot snapshot =
			world.BuildRenderFrameSnapshot(common::time::Clock::now());

		tests::Expect(result, joinResult, "ClientWorld: render bullet effect test join accepted");
		tests::Expect(result, snapshot.bulletStateList.size() == 1, "ClientWorld: render frame bullet count");
		tests::Expect(result, snapshot.impactEffectStateList.size() == 1, "ClientWorld: render frame effect count");

		if (snapshot.bulletStateList.size() == 1)
		{
			const client::game::ClientWorld::RenderBulletState& bulletState = snapshot.bulletStateList.front();

			tests::Expect(result, bulletState.bulletId == 300, "ClientWorld: render bullet id");
			tests::Expect(result, bulletState.x == 400.0F, "ClientWorld: render bullet x");
			tests::Expect(result, bulletState.y == 500.0F, "ClientWorld: render bullet y");
		}

		if (snapshot.impactEffectStateList.size() == 1)
		{
			const client::game::ClientWorld::RenderImpactEffectState& effectState = snapshot.impactEffectStateList.front();

			tests::Expect(result, effectState.effectType == common::game::EffectType::Impact, "ClientWorld: render effect type");
			tests::Expect(result, effectState.x == 410.0F, "ClientWorld: render effect x");
			tests::Expect(result, effectState.y == 510.0F, "ClientWorld: render effect y");
			tests::Expect(result, effectState.remainingSeconds > 0.0F, "ClientWorld: render effect duration initialized");
		}
	}

	void RunClearRenderFrameTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;

		world.TrySetJoinState(100, 2, 120.0F, 240.0F);

		client::game::ClientWorld::BulletStateDataList bulletStateDataList;
		bulletStateDataList.push_back(common::packet::BulletStateData{
			.bulletId = 300,
			.x = 400.0F,
			.y = 500.0F,
			});

		world.ApplyBulletSnapshotData(10, 2, bulletStateDataList);

		world.Clear();

		const client::game::ClientWorld::RenderFrameSnapshot snapshot =
			world.BuildRenderFrameSnapshot(common::time::Clock::now());

		tests::Expect(result, snapshot.localPlayerId == 0, "ClientWorld: cleared render frame player id");
		tests::Expect(result, snapshot.currentRoomId == 0, "ClientWorld: cleared render frame room id");
		tests::Expect(result, snapshot.lastServerTick == 0, "ClientWorld: cleared render frame server tick");
		tests::Expect(result, snapshot.playerStateList.empty(), "ClientWorld: cleared render player states");
		tests::Expect(result, snapshot.bulletStateList.empty(), "ClientWorld: cleared render bullet states");
		tests::Expect(result, snapshot.impactEffectStateList.empty(), "ClientWorld: cleared render effect states");
	}
}

namespace tests::client
{
	DebugTestResult RunClientWorldTests()
	{
		DebugTestResult result{};

		RunValidJoinStateAcceptedTest(result);
		RunZeroPlayerIdRejectedTest(result);
		RunInvalidRoomIdRejectedTest(result);
		RunDuplicateJoinStateRejectedTest(result);
		RunClearAllowsNewJoinStateTest(result);

		RunLocalPredictionAppliedToRenderFrameTest(result);
		RunLocalPredictionRequiresJoinTest(result);

		RunRenderFrameMetadataTest(result);
		RunRenderFramePlayerStateTest(result);
		RunRenderFrameBulletAndEffectTest(result);
		RunClearRenderFrameTest(result);

		return result;
	}
}