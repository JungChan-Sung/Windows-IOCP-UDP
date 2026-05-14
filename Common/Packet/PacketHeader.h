#pragma once

#include <cstdint>

#include "PacketConstants.h"
#include "PacketType.h"

namespace common::packet
{
	struct PacketHeader
	{
	public:
		std::uint16_t size = 0;
		PacketType type = PacketType::None;
		std::uint16_t version = protocolVersion;
	};
}