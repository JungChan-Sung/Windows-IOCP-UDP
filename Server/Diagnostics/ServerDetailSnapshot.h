#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Identity/IdentityTypes.h>

namespace server::diagnostics
{
	struct ServerPlayerDetailSnapshot
	{
		common::game::PlayerId playerId = 0;
		common::identity::AccountId accountId = 0;
		common::identity::PersistentPlayerId persistentPlayerId = 0;

		std::string nickname;

		common::game::RoomId roomId = 0;

		std::uint32_t lastAcceptedInputSequence = 0;
		std::uint32_t lastProcessedInputSequence = 0;
	};

	struct ServerRoomDetailSnapshot
	{
		common::game::RoomId roomId = 0;
		std::size_t memberCount = 0;
	};

	using ServerPlayerDetailSnapshotList = std::vector<ServerPlayerDetailSnapshot>;
	using ServerRoomDetailSnapshotList = std::vector<ServerRoomDetailSnapshot>;

	struct ServerDetailSnapshot
	{
		ServerPlayerDetailSnapshotList playerList;
		ServerRoomDetailSnapshotList roomList;
	};
}