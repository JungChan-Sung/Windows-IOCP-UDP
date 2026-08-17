#pragma once

#include <cstddef>

#include <Common/Time/TimeTypes.h>

namespace common::net
{
	struct ReliableUdpConfig
	{
	public:
		std::size_t maxPendingPacketCount = 64;
		int maxResendCount = 10;
		common::time::Milliseconds resendInterval = common::time::Milliseconds(100);
	};
}