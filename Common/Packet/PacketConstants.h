#pragma once

#include <cstddef>
#include <cstdint>

namespace common::packet
{
	inline constexpr std::uint8_t uint8WireSize = 1;
	inline constexpr std::uint8_t uint16WireSize = 2;
	inline constexpr std::uint8_t uint32WireSize = 4;
	inline constexpr std::uint8_t int32WireSize = 4;
	inline constexpr std::uint8_t floatWireSize = 4;

	inline constexpr std::uint16_t protocolVersion = 2;
	inline constexpr std::uint16_t packetHeaderReliableFlag = 0x8000;
	inline constexpr std::uint16_t packetHeaderVersionMask = 0x7FFF;

	inline constexpr std::size_t packetTypeWireSize = uint16WireSize;

	inline constexpr std::size_t serializedPacketHeaderSize
		= uint16WireSize
		+ packetTypeWireSize
		+ uint16WireSize;
	inline constexpr std::size_t maxSerializedPacketSize = 1200;
}