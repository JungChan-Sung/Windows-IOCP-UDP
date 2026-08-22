#include "GamePacketMapper.h"

namespace server::protocol
{
	common::packet::JoinResponsePacket BuildJoinResponse(const service::PeerSessionService::JoinResult& joinResult)
	{
		return common::packet::JoinResponsePacket{
			.playerId = joinResult.playerId,
			.roomId = joinResult.roomId,
			.spawnX = joinResult.spawnPosition.x,
			.spawnY = joinResult.spawnPosition.y,
		};
	}

	common::packet::JoinRoomResponsePacket BuildJoinRoomResponse(const service::PeerSessionService::RoomChangeResult& roomChangeResult)
	{
		return common::packet::JoinRoomResponsePacket{
			.roomId = roomChangeResult.nextRoomId,
			.spawnX = roomChangeResult.spawnPosition.x,
			.spawnY = roomChangeResult.spawnPosition.y,
		};
	}

	common::packet::PlayerJoinedPacket BuildPlayerJoined(common::game::PlayerId playerId, common::game::RoomId roomId, float x, float y)
	{
		return common::packet::PlayerJoinedPacket{
			.playerId = playerId,
			.roomId = roomId,
			.x = x,
			.y = y,
		};
	}

	common::packet::PlayerLeftPacket BuildPlayerLeft(common::game::PlayerId playerId, common::game::RoomId roomId)
	{
		return common::packet::PlayerLeftPacket{
			.playerId = playerId,
			.roomId = roomId,
		};
	}
}