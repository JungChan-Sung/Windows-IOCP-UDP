#pragma once

#include <Common/Game/InputFlags.h>

namespace client::input
{
	struct InputSnapshot
	{
	public:
		common::game::InputFlags movementFlags = common::game::InputFlags::None;

		bool isRoom1Pressed = false;
		bool isRoom2Pressed = false;
		bool isRoom3Pressed = false;

		bool fireRequested = false;

		bool toggleInterpolationRequested = false;
		bool togglePredictionRequested = false;
		bool toggleReconciliationRequested = false;

		bool decreaseInterpolationRequested = false;
		bool increaseInterpolationRequested = false;
	};
}