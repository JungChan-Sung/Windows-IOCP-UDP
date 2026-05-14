#pragma once

#include <Common/Packet/GamePacket.h>

namespace client::game
{
	[[nodiscard]] inline float GetEffectDurationSeconds(common::packet::EffectType effectType) noexcept
	{
		switch (effectType)
		{
		case common::packet::EffectType::Impact:
			return 0.20F;

		case common::packet::EffectType::Spawn:
			return 0.35F;

		default:
			return 0.20F;
		}
	}
}