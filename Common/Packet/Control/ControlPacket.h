#pragma once

#include <Common/Packet/PacketHeader.h>

namespace common::packet
{
	struct KeepAlivePacket
	{
	public:
		PacketHeader header{ 0, PacketType::KeepAlive };
	};
}