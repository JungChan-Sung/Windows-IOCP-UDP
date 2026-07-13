#pragma once

#include <cstddef>
#include <cstdint>

#include <Common/Net/Reliable/ReliableUdpProtocol.h>

namespace common::net
{
	inline constexpr std::size_t reliableUdpPacketHeaderWireSize = 12;

	struct ReliableUdpPacketHeader
	{
	public:
		ReliableSequence sequence = 0;
		ReliableSequence ackSequence = 0;
		std::uint32_t ackBitfield = 0;
	};
}