#include "ClientWorldSnapshotOrderTests.h"

#include <cmath>
#include <cstdint>
#include <limits>

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
		client::game::ClientWorld::RoomId roomId,
		float x,
		float y,
		bool isDead
	)
	{
		common::packet::PlayerSnapshotPacket packet{};
		packet.serverTick = serverTick;
		packet.roomId = roomId;
		packet.lastProcessedInputSequence = 0;
		packet.playerCount = 1;

		common::packet::PlayerStateData& playerStateData = packet.players[0];
		playerStateData.playerId = 100;
		playerStateData.x = x;
		playerStateData.y = y;
		playerStateData.hp = isDead ? 0 : 100;
		playerStateData.isDead = isDead ? 1 : 0;

		return packet;
	}

	[[nodiscard]] common::packet::PlayerSnapshotPacket MakeTwoPlayerSnapshot(
		std::uint32_t serverTick,
		client::game::ClientWorld::RoomId roomId,
		float localX,
		float localY,
		float remoteX,
		float remoteY
	)
	{
		common::packet::PlayerSnapshotPacket packet{};
		packet.serverTick = serverTick;
		packet.roomId = roomId;
		packet.lastProcessedInputSequence = 0;
		packet.playerCount = 2;

		common::packet::PlayerStateData& localPlayerStateData =
			packet.players[0];

		localPlayerStateData.playerId = 100;
		localPlayerStateData.x = localX;
		localPlayerStateData.y = localY;
		localPlayerStateData.hp = 100;
		localPlayerStateData.isDead = 0;

		common::packet::PlayerStateData& remotePlayerStateData =
			packet.players[1];

		remotePlayerStateData.playerId = 200;
		remotePlayerStateData.x = remoteX;
		remotePlayerStateData.y = remoteY;
		remotePlayerStateData.hp = 100;
		remotePlayerStateData.isDead = 0;

		return packet;
	}

	void InitializeLocalPlayer(
		client::game::ClientWorld& world,
		client::game::ClientWorld::RoomId roomId
	)
	{
		world.TrySetJoinState(
			100,
			roomId,
			320.0F,
			350.0F
		);

		client::game::ClientWorld::PlayerJoinedEvent
			playerJoinedEvent{};

		playerJoinedEvent.playerId = 100;
		playerJoinedEvent.x = 320.0F;
		playerJoinedEvent.y = 350.0F;

		world.ApplyPlayerJoinedEvent(playerJoinedEvent);
	}

	void RunOlderSnapshotIgnoredTest(
		tests::DebugTestResult& result
	)
	{
		client::game::ClientWorld world;
		InitializeLocalPlayer(world, 1);

		const common::packet::PlayerSnapshotPacket currentSnapshot =
			MakeLocalPlayerSnapshot(
				10,
				1,
				320.0F,
				350.0F,
				false
			);

		world.ApplyPlayerSnapshot(currentSnapshot);

		const common::packet::PlayerSnapshotPacket olderSnapshot =
			MakeLocalPlayerSnapshot(
				9,
				1,
				100.0F,
				350.0F,
				true
			);

		world.ApplyPlayerSnapshot(olderSnapshot);

		tests::Expect(
			result,
			world.GetLastServerTick() == 10,
			"ClientWorldSnapshotOrder: older snapshot does not replace server tick"
		);

		tests::Expect(
			result,
			!world.IsLocalPlayerDead(),
			"ClientWorldSnapshotOrder: older snapshot does not restore old dead state"
		);

		const client::game::ClientWorld::RenderFrameSnapshot renderSnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		tests::Expect(
			result,
			renderSnapshot.playerStateList.size() == 1,
			"ClientWorldSnapshotOrder: older snapshot test has one render player"
		);

		if (renderSnapshot.playerStateList.size() != 1)
		{
			return;
		}

		tests::Expect(
			result,
			IsNearlyEqual(
				renderSnapshot.playerStateList.front().x,
				320.0F
			),
			"ClientWorldSnapshotOrder: older snapshot does not restore old position"
		);
	}

	void RunDuplicateSnapshotIgnoredTest(
		tests::DebugTestResult& result
	)
	{
		client::game::ClientWorld world;
		InitializeLocalPlayer(world, 1);

		const common::packet::PlayerSnapshotPacket firstSnapshot =
			MakeLocalPlayerSnapshot(
				10,
				1,
				320.0F,
				350.0F,
				false
			);

		world.ApplyPlayerSnapshot(firstSnapshot);

		const common::packet::PlayerSnapshotPacket duplicateSnapshot =
			MakeLocalPlayerSnapshot(
				10,
				1,
				200.0F,
				350.0F,
				true
			);

		world.ApplyPlayerSnapshot(duplicateSnapshot);

		tests::Expect(
			result,
			world.GetLastServerTick() == 10,
			"ClientWorldSnapshotOrder: duplicate snapshot preserves server tick"
		);

		tests::Expect(
			result,
			!world.IsLocalPlayerDead(),
			"ClientWorldSnapshotOrder: duplicate snapshot does not change lifecycle state"
		);
	}

	void RunWrappedServerTickAcceptedTest(
		tests::DebugTestResult& result
	)
	{
		client::game::ClientWorld world;
		InitializeLocalPlayer(world, 1);

		constexpr std::uint32_t maxServerTick =
			std::numeric_limits<std::uint32_t>::max();

		const common::packet::PlayerSnapshotPacket maxTickSnapshot =
			MakeLocalPlayerSnapshot(
				maxServerTick,
				1,
				320.0F,
				350.0F,
				false
			);

		world.ApplyPlayerSnapshot(maxTickSnapshot);

		const common::packet::PlayerSnapshotPacket wrappedSnapshot =
			MakeLocalPlayerSnapshot(
				0,
				1,
				300.0F,
				350.0F,
				true
			);

		world.ApplyPlayerSnapshot(wrappedSnapshot);

		tests::Expect(
			result,
			world.GetLastServerTick() == 0,
			"ClientWorldSnapshotOrder: wrapped zero server tick accepted"
		);

		tests::Expect(
			result,
			world.IsLocalPlayerDead(),
			"ClientWorldSnapshotOrder: wrapped snapshot state applied"
		);

		const client::game::ClientWorld::RenderFrameSnapshot renderSnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		tests::Expect(
			result,
			renderSnapshot.playerStateList.size() == 1,
			"ClientWorldSnapshotOrder: wrapped snapshot test has one render player"
		);

		if (renderSnapshot.playerStateList.size() != 1)
		{
			return;
		}

		tests::Expect(
			result,
			IsNearlyEqual(
				renderSnapshot.playerStateList.front().x,
				300.0F
			),
			"ClientWorldSnapshotOrder: wrapped snapshot position applied"
		);
	}

	void RunDifferentRoomSnapshotIgnoredTest(
		tests::DebugTestResult& result
	)
	{
		client::game::ClientWorld world;
		InitializeLocalPlayer(world, 2);

		const common::packet::PlayerSnapshotPacket wrongRoomSnapshot =
			MakeLocalPlayerSnapshot(
				100,
				1,
				100.0F,
				350.0F,
				true
			);

		world.ApplyPlayerSnapshot(wrongRoomSnapshot);

		tests::Expect(
			result,
			world.GetCurrentRoomId() == 2,
			"ClientWorldSnapshotOrder: different room snapshot does not replace room"
		);

		tests::Expect(
			result,
			world.GetLastServerTick() == 0,
			"ClientWorldSnapshotOrder: different room snapshot does not advance server tick"
		);

		tests::Expect(
			result,
			!world.IsLocalPlayerDead(),
			"ClientWorldSnapshotOrder: different room snapshot does not change player state"
		);

		const common::packet::PlayerSnapshotPacket validSnapshot =
			MakeLocalPlayerSnapshot(
				1,
				2,
				320.0F,
				350.0F,
				false
			);

		world.ApplyPlayerSnapshot(validSnapshot);

		tests::Expect(
			result,
			world.GetLastServerTick() == 1,
			"ClientWorldSnapshotOrder: valid room snapshot accepted after rejected room snapshot"
		);
	}

	void RunRecoverySnapshotReanchorsInterpolationTest(
		tests::DebugTestResult& result
	)
	{
		client::game::ClientWorld world;
		InitializeLocalPlayer(world, 1);

		client::game::ClientWorld::PlayerJoinedEvent
			remotePlayerJoinedEvent{};

		remotePlayerJoinedEvent.playerId = 200;
		remotePlayerJoinedEvent.x = 100.0F;
		remotePlayerJoinedEvent.y = 350.0F;

		world.ApplyPlayerJoinedEvent(remotePlayerJoinedEvent);

		const common::packet::PlayerSnapshotPacket initialSnapshot =
			MakeTwoPlayerSnapshot(
				10,
				1,
				320.0F,
				350.0F,
				100.0F,
				350.0F
			);

		world.ApplyPlayerSnapshot(initialSnapshot);

		world.BeginRecovery();

		const bool recovered =
			world.TrySetJoinState(
				100,
				1,
				500.0F,
				350.0F
			);

		tests::Expect(
			result,
			recovered,
			"ClientWorldSnapshotOrder: recovery join succeeds"
		);

		const common::packet::PlayerSnapshotPacket recoveredSnapshot =
			MakeTwoPlayerSnapshot(
				400,
				1,
				500.0F,
				350.0F,
				700.0F,
				350.0F
			);

		world.ApplyPlayerSnapshot(recoveredSnapshot);

		tests::Expect(
			result,
			world.GetLastServerTick() == 400,
			"ClientWorldSnapshotOrder: recovered snapshot advances server tick"
		);

		const client::game::ClientWorld::RenderFrameSnapshot renderSnapshot =
			world.BuildRenderFrameSnapshot(
				common::time::Clock::now()
			);

		const client::game::ClientWorld::RenderPlayerState*
			remotePlayerState = nullptr;

		for (
			const client::game::ClientWorld::RenderPlayerState& playerState
			: renderSnapshot.playerStateList
			)
		{
			if (playerState.playerId != 200)
			{
				continue;
			}

			remotePlayerState = &playerState;
			break;
		}

		tests::Expect(
			result,
			remotePlayerState != nullptr,
			"ClientWorldSnapshotOrder: recovered remote player exists"
		);

		if (remotePlayerState == nullptr)
		{
			return;
		}

		tests::Expect(
			result,
			IsNearlyEqual(
				remotePlayerState->x,
				700.0F
			),
			"ClientWorldSnapshotOrder: recovery snapshot reanchors remote interpolation"
		);
	}
}

namespace tests::client
{
	DebugTestResult RunClientWorldSnapshotOrderTests()
	{
		DebugTestResult result{};

		RunOlderSnapshotIgnoredTest(result);
		RunDuplicateSnapshotIgnoredTest(result);
		RunWrappedServerTickAcceptedTest(result);
		RunDifferentRoomSnapshotIgnoredTest(result);
		RunRecoverySnapshotReanchorsInterpolationTest(result);

		return result;
	}
}