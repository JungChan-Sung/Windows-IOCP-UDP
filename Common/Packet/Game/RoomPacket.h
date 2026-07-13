#pragma once

#include <cstdint>

#include <Common/Packet/PacketHeader.h>

namespace common::packet
{
	struct JoinRoomRequestPacket
	{
	public:
		PacketHeader header{ 0, PacketType::JoinRoomRequest };
		std::int32_t roomId = 0;
	};

	struct JoinRoomResponsePacket
	{
	public:
		PacketHeader header{ 0, PacketType::JoinRoomResponse };
		std::int32_t roomId = 0;
		float spawnX = 0.0F;
		float spawnY = 0.0F;
	};

	struct PlayerJoinedPacket
	{
	public:
		PacketHeader header{ 0, PacketType::PlayerJoined };
		std::uint32_t playerId = 0;
		std::int32_t roomId = 0;
		float x = 0.0F;
		float y = 0.0F;
	};

	struct PlayerLeftPacket
	{
	public:
		PacketHeader header{ 0, PacketType::PlayerLeft };
		std::uint32_t playerId = 0;
		std::int32_t roomId = 0;
	};
}