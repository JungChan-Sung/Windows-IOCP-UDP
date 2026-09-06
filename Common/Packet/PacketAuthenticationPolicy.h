#pragma once

#include <Common/Packet/PacketType.h>

namespace common::packet
{
	// 로그인과 세션 등록에 필요한 초기 요청을 제외한 클라이언트 패킷에 인증을 요구하는 함수
	[[nodiscard]] inline constexpr bool RequiresClientPacketAuthentication(PacketType packetType) noexcept
	{
		switch (packetType)
		{
		case PacketType::AccountLoginRequest:
		case PacketType::JoinRequest:
			return false;

		default:
			return true;
		}
	}
}