#pragma once

#include <Common/Game/GameTypes.h>
#include <Common/Packet/Game/GamePacket.h>

namespace server::game
{
	struct ImpactEffectState
	{
	public:
		common::game::RoomId roomId = 0;
		common::packet::EffectType effectType = common::packet::EffectType::None;
		float x = 0.0F;
		float y = 0.0F;
	};
}