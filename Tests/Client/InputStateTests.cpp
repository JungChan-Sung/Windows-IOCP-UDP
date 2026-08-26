#include "InputStateTests.h"

#include <Windows.h>

#include <Common/Game/InputFlags.h>

#include <Client/Input/InputSnapshot.h>
#include <Client/Input/InputState.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void RunMovementHeldStateTest(tests::DebugTestResult& result)
	{
		client::input::InputState inputState;

		inputState.SetKeyDown('W', false);
		inputState.SetKeyDown('A', false);

		const client::input::InputSnapshot firstSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(
			result,
			firstSnapshot.movementFlags == (common::game::InputFlags::Up | common::game::InputFlags::Left),
			"InputState: movement pressed"
		);

		const client::input::InputSnapshot secondSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(
			result,
			secondSnapshot.movementFlags == (common::game::InputFlags::Up | common::game::InputFlags::Left),
			"InputState: held movement persists after consume"
		);

		inputState.SetKeyUp('W');

		const client::input::InputSnapshot thirdSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(
			result,
			thirdSnapshot.movementFlags == common::game::InputFlags::Left,
			"InputState: released movement removed"
		);

		inputState.SetKeyUp('A');

		const client::input::InputSnapshot fourthSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(
			result,
			fourthSnapshot.movementFlags == common::game::InputFlags::None,
			"InputState: all movement released"
		);
	}

	void RunMovementAliasTest(tests::DebugTestResult& result)
	{
		client::input::InputState inputState;

		inputState.SetKeyDown(VK_UP, false);
		inputState.SetKeyDown(VK_RIGHT, false);

		const client::input::InputSnapshot snapshot = inputState.ConsumeSnapshot();

		tests::Expect(
			result,
			snapshot.movementFlags == (common::game::InputFlags::Up | common::game::InputFlags::Right),
			"InputState: arrow key movement aliases"
		);

		inputState.SetKeyUp(VK_UP);
		inputState.SetKeyUp(VK_RIGHT);

		tests::Expect(
			result,
			inputState.ConsumeSnapshot().movementFlags == common::game::InputFlags::None,
			"InputState: arrow key movement released"
		);
	}

	void RunRoomHeldStateTest(tests::DebugTestResult& result)
	{
		client::input::InputState inputState;

		inputState.SetKeyDown('2', false);

		const client::input::InputSnapshot firstSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(result, !firstSnapshot.isRoom1Pressed, "InputState: room 1 not pressed");
		tests::Expect(result, firstSnapshot.isRoom2Pressed, "InputState: room 2 pressed");
		tests::Expect(result, !firstSnapshot.isRoom3Pressed, "InputState: room 3 not pressed");

		const client::input::InputSnapshot secondSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(result, secondSnapshot.isRoom2Pressed, "InputState: held room input persists after consume");

		inputState.SetKeyUp('2');

		const client::input::InputSnapshot thirdSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(result, !thirdSnapshot.isRoom2Pressed, "InputState: released room input removed");
	}

	void RunFireOneShotTest(tests::DebugTestResult& result)
	{
		client::input::InputState inputState;

		inputState.SetKeyDown(VK_SPACE, false);

		const client::input::InputSnapshot firstSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(result, firstSnapshot.fireRequested, "InputState: fire requested");

		const client::input::InputSnapshot secondSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(result, !secondSnapshot.fireRequested, "InputState: fire consumed once");
	}

	void RunRepeatedActionIgnoredTest(tests::DebugTestResult& result)
	{
		client::input::InputState inputState;

		inputState.SetKeyDown(VK_SPACE, true);
		inputState.SetKeyDown(VK_OEM_MINUS, true);
		inputState.SetKeyDown(VK_OEM_PLUS, true);

		const client::input::InputSnapshot snapshot = inputState.ConsumeSnapshot();

		tests::Expect(result, !snapshot.fireRequested, "InputState: repeated fire ignored");
		tests::Expect(result, !snapshot.decreaseInterpolationRequested, "InputState: repeated decrease interpolation ignored");
		tests::Expect(result, !snapshot.increaseInterpolationRequested, "InputState: repeated increase interpolation ignored");
	}

	void RunMultipleOneShotActionTest(tests::DebugTestResult& result)
	{
		client::input::InputState inputState;

		inputState.SetKeyDown(VK_SPACE, false);
		inputState.SetKeyDown(VK_OEM_MINUS, false);
		inputState.SetKeyDown(VK_OEM_PLUS, false);

		const client::input::InputSnapshot firstSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(result, firstSnapshot.fireRequested, "InputState: multiple actions preserve fire");
		tests::Expect(result, firstSnapshot.decreaseInterpolationRequested, "InputState: multiple actions preserve interpolation decrease");
		tests::Expect(result, firstSnapshot.increaseInterpolationRequested, "InputState: multiple actions preserve interpolation increase");

		const client::input::InputSnapshot secondSnapshot = inputState.ConsumeSnapshot();

		tests::Expect(result, !secondSnapshot.fireRequested, "InputState: multiple fire action consumed");
		tests::Expect(result, !secondSnapshot.decreaseInterpolationRequested, "InputState: decrease action consumed");
		tests::Expect(result, !secondSnapshot.increaseInterpolationRequested, "InputState: increase action consumed");
	}

	void RunClearTest(tests::DebugTestResult& result)
	{
		client::input::InputState inputState;

		inputState.SetKeyDown('W', false);
		inputState.SetKeyDown('3', false);
		inputState.SetKeyDown(VK_SPACE, false);
		inputState.SetKeyDown(VK_OEM_PLUS, false);

		inputState.Clear();

		const client::input::InputSnapshot snapshot = inputState.ConsumeSnapshot();

		tests::Expect(
			result,
			snapshot.movementFlags == common::game::InputFlags::None,
			"InputState: clear movement"
		);

		tests::Expect(result, !snapshot.isRoom1Pressed, "InputState: clear room 1");
		tests::Expect(result, !snapshot.isRoom2Pressed, "InputState: clear room 2");
		tests::Expect(result, !snapshot.isRoom3Pressed, "InputState: clear room 3");

		tests::Expect(result, !snapshot.fireRequested, "InputState: clear fire action");
		tests::Expect(result, !snapshot.decreaseInterpolationRequested, "InputState: clear interpolation decrease");
		tests::Expect(result, !snapshot.increaseInterpolationRequested, "InputState: clear interpolation increase");
	}

	void RunUnknownKeyTest(tests::DebugTestResult& result)
	{
		client::input::InputState inputState;

		inputState.SetKeyDown(VK_F12, false);
		inputState.SetKeyUp(VK_F12);

		const client::input::InputSnapshot snapshot = inputState.ConsumeSnapshot();

		tests::Expect(
			result,
			snapshot.movementFlags == common::game::InputFlags::None,
			"InputState: unknown key does not affect movement"
		);

		tests::Expect(result, !snapshot.fireRequested, "InputState: unknown key does not create action");
	}
}

namespace tests::client
{
	DebugTestResult RunInputStateTests()
	{
		DebugTestResult result{};

		RunMovementHeldStateTest(result);
		RunMovementAliasTest(result);
		RunRoomHeldStateTest(result);
		RunFireOneShotTest(result);
		RunRepeatedActionIgnoredTest(result);
		RunMultipleOneShotActionTest(result);
		RunClearTest(result);
		RunUnknownKeyTest(result);

		return result;
	}
}