#pragma once

#include <Common/Game/EffectType.h>

namespace client::game
{
	[[nodiscard]] inline float GetEffectDurationSeconds(common::game::EffectType effectType) noexcept
	{
		switch (effectType)
		{
		case common::game::EffectType::Impact:
			return 0.20F;

		case common::game::EffectType::Spawn:
			return 0.35F;

		default:
			return 0.20F;
		}
	}
}