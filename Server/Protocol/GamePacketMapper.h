#pragma once

#include <Common/Game/GameTypes.h>
#include <Common/Packet/Game/GamePacket.h>

#include <Server/Service/PeerSessionService.h>

namespace server::protocol
{
	[[nodiscard]] common::packet::JoinResponsePacket BuildJoinResponse(const service::PeerSessionService::JoinResult& joinResult);
	[[nodiscard]] common::packet::JoinRoomResponsePacket BuildJoinRoomResponse(const service::PeerSessionService::RoomChangeResult& roomChangeResult);
	[[nodiscard]] common::packet::PlayerJoinedPacket BuildPlayerJoined(common::game::PlayerId playerId, common::game::RoomId roomId, float x, float y);
	[[nodiscard]] common::packet::PlayerLeftPacket BuildPlayerLeft(common::game::PlayerId playerId, common::game::RoomId roomId);
}