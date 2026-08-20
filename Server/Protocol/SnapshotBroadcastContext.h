#pragma once

#include <cstdint>
#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Net/EndpointKey.h>

namespace server::protocol
{
	struct SnapshotPeerContext
	{
	public:
		common::net::EndpointKey endpointKey{};
		common::game::PlayerId playerId = 0;
		std::uint32_t lastInputSequence = 0;
	};

	using SnapshotPeerContextList = std::vector<SnapshotPeerContext>;

	struct SnapshotRoomContext
	{
	public:
		common::game::RoomId roomId = 0;
		SnapshotPeerContextList peerContextList;
	};

	using SnapshotRoomContextList = std::vector<SnapshotRoomContext>;
}