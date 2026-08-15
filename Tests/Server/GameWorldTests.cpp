#include "GameWorldTests.h"

#include <Common/Game/GameTypes.h>

#include <Server/Game/BulletState.h>
#include <Server/Game/GameWorld.h>
#include <Server/Game/ImpactEffectState.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void RunClearRoomTransientStateTest(tests::DebugTestResult& result)
	{
		constexpr common::game::RoomId targetRoomId = 1;
		constexpr common::game::RoomId otherRoomId = 2;

		server::game::GameWorld gameWorld;

		gameWorld.AddBullet(server::game::BulletState{
			.bulletId = 1,
			.roomId = targetRoomId,
			});

		gameWorld.AddBullet(server::game::BulletState{
			.bulletId = 2,
			.roomId = otherRoomId,
			});

		gameWorld.AddBullet(server::game::BulletState{
			.bulletId = 3,
			.roomId = targetRoomId,
			});

		gameWorld.AddPendingImpactEffect(server::game::ImpactEffectState{
			.roomId = targetRoomId,
			});

		gameWorld.AddPendingImpactEffect(server::game::ImpactEffectState{
			.roomId = otherRoomId,
			});

		gameWorld.AddPendingImpactEffect(server::game::ImpactEffectState{
			.roomId = targetRoomId,
			});

		tests::Expect(result, gameWorld.GetBulletCount() == 3, "GameWorld: initial bullet count");
		tests::Expect(result, gameWorld.GetPendingImpactEffectCount() == 3, "GameWorld: initial impact effect count");

		gameWorld.ClearRoomTransientState(targetRoomId);

		tests::Expect(result, gameWorld.GetBulletCount() == 1, "GameWorld: target room bullets removed");
		tests::Expect(result, gameWorld.GetPendingImpactEffectCount() == 1, "GameWorld: target room impact effects removed");

		const server::game::GameWorld::BulletStateList& bulletStateList = gameWorld.GetBulletStateList();
		tests::Expect(result, bulletStateList.size() == 1, "GameWorld: one bullet remains");

		if (bulletStateList.size() == 1)
		{
			tests::Expect(result, bulletStateList.front().bulletId == 2, "GameWorld: other room bullet preserved");
			tests::Expect(result, bulletStateList.front().roomId == otherRoomId, "GameWorld: remaining bullet room preserved");
		}

		const server::game::GameWorld::ImpactEffectStateList& impactEffectStateList = gameWorld.GetPendingImpactEffectStateList();
		tests::Expect(result, impactEffectStateList.size() == 1, "GameWorld: one impact effect remains");

		if (impactEffectStateList.size() == 1)
		{
			tests::Expect(result, impactEffectStateList.front().roomId == otherRoomId, "GameWorld: other room impact effect preserved");
		}

		gameWorld.ClearRoomTransientState(targetRoomId);

		tests::Expect(result, gameWorld.GetBulletCount() == 1, "GameWorld: repeated room clear preserves other bullets");
		tests::Expect(result, gameWorld.GetPendingImpactEffectCount() == 1, "GameWorld: repeated room clear preserves other effects");
	}
}

namespace tests::server
{
	DebugTestResult RunGameWorldTests()
	{
		DebugTestResult result{};

		RunClearRoomTransientStateTest(result);

		return result;
	}
}