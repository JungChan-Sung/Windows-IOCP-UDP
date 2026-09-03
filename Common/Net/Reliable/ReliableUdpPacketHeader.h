#pragma once

#include <cstddef>
#include <cstdint>

#include <Common/Net/Reliable/ReliableUdpProtocol.h>

namespace common::net
{
	inline constexpr std::size_t reliableUdpPacketHeaderWireSize = 12;

	//┌──────────┐
	//│ sequence     4 byte│
	//├──────────┤
	//│ ackSequence  4 byte│
	//├──────────┤
	//│ ackBitfield  4 byte│
	//└──────────┘
	// Total : 12 byte
	struct ReliableUdpPacketHeader
	{
	public:
		ReliableSequence sequence = 0;
		ReliableSequence ackSequence = 0;
		std::uint32_t ackBitfield = 0;		// ackSequence 이전 32개 sequence의 수신 여부(비트 단위)
	};
}