#pragma once

namespace common::packet
{
	template <typename TPacket>
	struct PacketCodec;

	template <typename TPacket>
	inline constexpr int packetExpectedSize = PacketCodec<TPacket>::fixedWireSize;
}