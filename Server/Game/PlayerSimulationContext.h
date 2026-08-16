#pragma once

#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Identity/IdentityTypes.h>

namespace server::game
{
	struct PlayerSimulationContext
	{
	public:
		common::game::PlayerId playerId = 0;
		common::identity::PersistentPlayerId persistentPlayerId = 0;
		common::game::RoomId roomId = 0;
	};

	using PlayerSimulationContextList = std::vector<PlayerSimulationContext>;
}