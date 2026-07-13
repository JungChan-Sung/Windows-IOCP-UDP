#pragma once

#include <cstdint>

#include <Common/Game/InputFlags.h>
#include <Common/Packet/PacketHeader.h>

namespace common::packet
{
	struct InputCommandPacket
	{
	public:
		PacketHeader header{ 0, PacketType::InputCommand };
		std::uint32_t inputSequence = 0;
		game::InputFlags inputFlags = game::InputFlags::None;
	};

	struct FireRequestPacket
	{
	public:
		PacketHeader header{ 0, PacketType::FireRequest };
	};
}