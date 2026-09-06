#pragma once

#include <cstdint>

#include "PacketConstants.h"
#include "PacketType.h"

namespace common::packet
{
	//┌────────┐
	//│ size     2 byte│
	//├────────┤
	//│ type     2 byte│
	//├────────┤
	//│ version  2 byte│
	//└────────┘
	// Total : 6 byte
	// 모든 패킷의 크기, 타입, 프로토콜 정보를 담는 구조체
	struct PacketHeader
	{
	public:
		std::uint16_t size = 0;
		PacketType type = PacketType::None;
		std::uint16_t version = protocolVersion;
	};
}