#pragma once

#include <cstdint>

#include <Common/Net/SessionToken.h>
#include <Common/Packet/PacketHeader.h>

namespace common::packet
{
	struct JoinRequestPacket
	{
	public:
		PacketHeader header{ 0, PacketType::JoinRequest };
		common::net::SessionToken sessionToken{};
	};

	struct JoinResponsePacket
	{
	public:
		PacketHeader header{ 0, PacketType::JoinResponse };
		std::uint32_t playerId = 0;
		std::int32_t roomId = 0;
		float spawnX = 0.0F;
		float spawnY = 0.0F;
	};

	struct LeaveRequestPacket
	{
	public:
		PacketHeader header{ 0, PacketType::LeaveRequest };
	};

	struct LeaveResponsePacket
	{
	public:
		PacketHeader header{ 0, PacketType::LeaveResponse };
	};
}