#pragma once

namespace common::net
{
	// IOCP 완료 통지에서 UDP 작업이 수신인지 송신인지 구분
	enum class UdpOperationType
	{
		Recv,
		Send
	};
}