#pragma once

#include <Common/Game/EffectType.h>
#include <Common/Game/GameTypes.h>

namespace server::game
{
	struct ImpactEffectState
	{
	public:
		common::game::RoomId roomId = 0;
		common::game::EffectType effectType = common::game::EffectType::None;
		float x = 0.0F;
		float y = 0.0F;
	};
}