#pragma once

#include <span>
#include <vector>

namespace common::packet
{
	using PacketByte = char;
	using PacketBuffer = std::vector<PacketByte>;
	using PacketSpan = std::span<PacketByte>;
	using ConstPacketSpan = std::span<const PacketByte>;
}