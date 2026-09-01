#include "ClientWorldLifecycleTests.h"

#include <cmath>
#include <cstdint>

#include <Common/Game/InputFlags.h>
#include <Common/Game/SimulationConstants.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Time/TimeTypes.h>

#include <Client/Game/ClientWorld.h>

namespace
{
	inline constexpr float floatTolerance = 0.001F;

	[[nodiscard]] bool IsNearlyEqual(float lhs, float rhs) noexcept
	{
		return std::abs(lhs - rhs) <= floatTolerance;
	}

	[[nodiscard]] common::packet::PlayerSnapshotPacket MakeLocalPlayerSnapshot(
		std::uint32_t serverTick,
		std::uint32_t lastProcessedInputSequence,
		float x,
		float y,
		bool isDead
	)
	{
		common::packet::PlayerSnapshotPacket packet{};
		packet.serverTick = serverTick;
		packet.roomId = 1;
		packet.lastProcessedInputSequence = lastProcessedInputSequence;
		packet.playerCount = 1;

		common::packet::PlayerStateData& playerStateData = packet.players[0];
		playerStateData.playerId = 100;
		playerStateData.x = x;
		playerStateData.y = y;
		playerStateData.hp = isDead ? 0 : 100;
		playerStateData.isDead = isDead ? 1 : 0;

		return packet;
	}

	void InitializeLocalPlayer(client::game::ClientWorld& world)
	{
		world.TrySetJoinState(100, 1, 320.0F, 350.0F);

		client::game::ClientWorld::PlayerJoinedEvent playerJoinedEvent{};
		playerJoinedEvent.playerId = 100;
		playerJoinedEvent.x = 320.0F;
		playerJoinedEvent.y = 350.0F;

		world.ApplyPlayerJoinedEvent(playerJoinedEvent);
	}

	void RunDeathResetsPredictionTest(tests::DebugTestResult& result)
	{
		client::game::ClientWorld world;
		InitializeLocalPlayer(world);

		world.ApplyLocalPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds
		);

		const common::packet::PlayerSnapshotPacket deathSnapshot =
			MakeLocalPlayerSnapshot(
				1,
				0,
				300.0F,
				350.0F,
				true
			);

		world.ApplyPlayerSnapshot(deathSnapshot);

		const client::game::ClientWorld::RenderFrameSnapshot renderSnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		tests::Expect(
			result,
			world.IsLocalPlayerDead(),
			"ClientWorldLifecycle: local player enters dead state"
		);

		tests::Expect(
			result,
			renderSnapshot.playerStateList.size() == 1,
			"ClientWorldLifecycle: death snapshot has one render player"
		);

		if (renderSnapshot.playerStateList.size() != 1)
		{
			return;
		}

		const client::game::ClientWorld::RenderPlayerState&
			renderPlayerState =
			renderSnapshot.playerStateList.front();

		tests::Expect(
			result,
			IsNearlyEqual(renderPlayerState.x, 300.0F),
			"ClientWorldLifecycle: death resets prediction to authoritative x"
		);

		tests::Expect(
			result,
			IsNearlyEqual(renderPlayerState.y, 350.0F),
			"ClientWorldLifecycle: death resets prediction to authoritative y"
		);
	}

	void RunDeadPlayerPredictionIgnoredTest(
		tests::DebugTestResult& result
	)
	{
		client::game::ClientWorld world;
		InitializeLocalPlayer(world);

		const common::packet::PlayerSnapshotPacket deathSnapshot =
			MakeLocalPlayerSnapshot(
				1,
				0,
				300.0F,
				350.0F,
				true
			);

		world.ApplyPlayerSnapshot(deathSnapshot);

		world.ApplyLocalPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds
		);

		const client::game::ClientWorld::RenderFrameSnapshot renderSnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		tests::Expect(
			result,
			renderSnapshot.playerStateList.size() == 1,
			"ClientWorldLifecycle: dead prediction test has one render player"
		);

		if (renderSnapshot.playerStateList.size() != 1)
		{
			return;
		}

		const client::game::ClientWorld::RenderPlayerState&
			renderPlayerState =
			renderSnapshot.playerStateList.front();

		tests::Expect(
			result,
			IsNearlyEqual(renderPlayerState.x, 300.0F),
			"ClientWorldLifecycle: dead player prediction is ignored"
		);
	}

	void RunRespawnResetsPredictionTest(
		tests::DebugTestResult& result
	)
	{
		client::game::ClientWorld world;
		InitializeLocalPlayer(world);

		world.ApplyLocalPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds
		);

		const common::packet::PlayerSnapshotPacket deathSnapshot =
			MakeLocalPlayerSnapshot(
				1,
				0,
				300.0F,
				350.0F,
				true
			);

		world.ApplyPlayerSnapshot(deathSnapshot);

		const common::packet::PlayerSnapshotPacket respawnSnapshot =
			MakeLocalPlayerSnapshot(
				2,
				0,
				400.0F,
				350.0F,
				false
			);

		world.ApplyPlayerSnapshot(respawnSnapshot);

		const client::game::ClientWorld::RenderFrameSnapshot
			respawnRenderSnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		tests::Expect(
			result,
			!world.IsLocalPlayerDead(),
			"ClientWorldLifecycle: local player leaves dead state after respawn"
		);

		tests::Expect(
			result,
			respawnRenderSnapshot.playerStateList.size() == 1,
			"ClientWorldLifecycle: respawn snapshot has one render player"
		);

		if (respawnRenderSnapshot.playerStateList.size() != 1)
		{
			return;
		}

		const client::game::ClientWorld::RenderPlayerState&
			respawnPlayerState =
			respawnRenderSnapshot.playerStateList.front();

		tests::Expect(
			result,
			IsNearlyEqual(respawnPlayerState.x, 400.0F),
			"ClientWorldLifecycle: respawn resets prediction to authoritative x"
		);

		tests::Expect(
			result,
			IsNearlyEqual(respawnPlayerState.y, 350.0F),
			"ClientWorldLifecycle: respawn resets prediction to authoritative y"
		);

		world.ApplyLocalPredictionTick(
			2,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds
		);

		const client::game::ClientWorld::RenderFrameSnapshot
			predictedRenderSnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		const float expectedX =
			400.0F
			+ (
				common::game::defaultMoveSpeed
				* common::game::defaultFixedDeltaSeconds
				);

		tests::Expect(
			result,
			IsNearlyEqual(
				predictedRenderSnapshot.playerStateList.front().x,
				expectedX
			),
			"ClientWorldLifecycle: prediction resumes after respawn"
		);
	}

	void RunRecoveryPreservesIdentityAndAllowsRejoinTest(
		tests::DebugTestResult& result
	)
	{
		client::game::ClientWorld world;
		InitializeLocalPlayer(world);

		world.ApplyLocalPredictionTick(
			1,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds
		);

		const client::game::ClientWorld::RenderFrameSnapshot
			beforeRecoverySnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		tests::Expect(
			result,
			world.IsJoined(),
			"ClientWorldLifecycle: recovery setup joined"
		);

		tests::Expect(
			result,
			beforeRecoverySnapshot.playerStateList.size() == 1,
			"ClientWorldLifecycle: recovery setup render player exists"
		);

		world.BeginRecovery();

		tests::Expect(
			result,
			!world.IsJoined(),
			"ClientWorldLifecycle: recovery marks world unjoined"
		);

		tests::Expect(
			result,
			world.GetLocalPlayerId() == 100,
			"ClientWorldLifecycle: recovery preserves local player id"
		);

		tests::Expect(
			result,
			world.GetCurrentRoomId() == 1,
			"ClientWorldLifecycle: recovery preserves room id"
		);

		const client::game::ClientWorld::RenderFrameSnapshot
			duringRecoverySnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		tests::Expect(
			result,
			duringRecoverySnapshot.playerStateList.size() == 1,
			"ClientWorldLifecycle: recovery preserves render state"
		);

		world.ApplyLocalPredictionTick(
			2,
			common::game::InputFlags::Right,
			common::game::defaultFixedDeltaSeconds
		);

		const client::game::ClientWorld::RenderFrameSnapshot
			afterIgnoredPredictionSnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		if (!duringRecoverySnapshot.playerStateList.empty()
			&& !afterIgnoredPredictionSnapshot.playerStateList.empty())
		{
			tests::Expect(
				result,
				IsNearlyEqual(
					duringRecoverySnapshot.playerStateList.front().x,
					afterIgnoredPredictionSnapshot.playerStateList.front().x
				),
				"ClientWorldLifecycle: recovery ignores local prediction"
			);
		}

		const bool differentPlayerRejoinAccepted =
			world.TrySetJoinState(
				200,
				1,
				500.0F,
				500.0F
			);

		tests::Expect(
			result,
			!differentPlayerRejoinAccepted,
			"ClientWorldLifecycle: recovery rejects different player id"
		);

		tests::Expect(
			result,
			!world.IsJoined(),
			"ClientWorldLifecycle: rejected recovery remains unjoined"
		);

		const bool recovered =
			world.TrySetJoinState(
				100,
				1,
				300.0F,
				340.0F
			);

		tests::Expect(
			result,
			recovered,
			"ClientWorldLifecycle: recovery accepts preserved player id"
		);

		tests::Expect(
			result,
			world.IsJoined(),
			"ClientWorldLifecycle: recovered world becomes joined"
		);

		tests::Expect(
			result,
			world.GetLocalPlayerId() == 100,
			"ClientWorldLifecycle: recovered local player id preserved"
		);

		tests::Expect(
			result,
			world.GetCurrentRoomId() == 1,
			"ClientWorldLifecycle: recovered room id preserved"
		);

		const client::game::ClientWorld::RenderFrameSnapshot
			recoveredSnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		tests::Expect(
			result,
			recoveredSnapshot.playerStateList.size() == 1,
			"ClientWorldLifecycle: recovered render player exists"
		);

		if (recoveredSnapshot.playerStateList.size() == 1)
		{
			const client::game::ClientWorld::RenderPlayerState&
				playerState =
				recoveredSnapshot.playerStateList.front();

			tests::Expect(
				result,
				IsNearlyEqual(playerState.x, 300.0F),
				"ClientWorldLifecycle: recovery resets local x to server state"
			);

			tests::Expect(
				result,
				IsNearlyEqual(playerState.y, 340.0F),
				"ClientWorldLifecycle: recovery resets local y to server state"
			);
		}
	}
}

namespace tests::client
{
	DebugTestResult RunClientWorldLifecycleTests()
	{
		DebugTestResult result{};

		RunDeathResetsPredictionTest(result);
		RunDeadPlayerPredictionIgnoredTest(result);
		RunRespawnResetsPredictionTest(result);
		RunRecoveryPreservesIdentityAndAllowsRejoinTest(result);

		return result;
	}
}