#pragma once

#include <cstdint>
#include <vector>

#include <Common/Game/GameTypes.h>

namespace server::game
{
	struct KillEvent
	{
	public:
		common::game::PlayerId killerPlayerId = 0;
		std::int64_t killerPersistentPlayerId = 0;

		common::game::PlayerId victimPlayerId = 0;
		std::int64_t victimPersistentPlayerId = 0;

		common::game::RoomId roomId = 0;
	};

	using KillEventList = std::vector<KillEvent>;
}