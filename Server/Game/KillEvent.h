#pragma once

#include <cstdint>
#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Identity/IdentityTypes.h>

namespace server::game
{
	struct KillEvent
	{
	public:
		common::game::PlayerId killerPlayerId = 0;
		common::identity::PersistentPlayerId killerPersistentPlayerId = 0;

		common::game::PlayerId victimPlayerId = 0;
		common::identity::PersistentPlayerId victimPersistentPlayerId = 0;

		common::game::RoomId roomId = 0;
	};

	using KillEventList = std::vector<KillEvent>;
}