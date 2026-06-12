#include "ClientWorldTests.h"

#include <Client/Game/ClientWorld.h>

#include <Tests/DebugTestResult.h>

namespace
{
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
}

namespace tests::client
{
	tests::DebugTestResult RunClientWorldTests()
	{
		tests::DebugTestResult result{};

		RunValidJoinStateAcceptedTest(result);
		RunZeroPlayerIdRejectedTest(result);
		RunInvalidRoomIdRejectedTest(result);
		RunDuplicateJoinStateRejectedTest(result);
		RunClearAllowsNewJoinStateTest(result);

		return result;
	}
}