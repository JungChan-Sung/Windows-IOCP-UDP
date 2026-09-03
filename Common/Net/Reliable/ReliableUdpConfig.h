#pragma once

#include <cstddef>

#include <Common/Time/TimeTypes.h>

namespace common::net
{
	// Reliable UDP - 송신 윈도우 크기와 재전송 정책 구조체
	struct ReliableUdpConfig
	{
	public:
		std::size_t maxPendingPacketCount = 64;
		int maxResendCount = 10;
		common::time::Milliseconds resendInterval = common::time::Milliseconds(100);
	};
}