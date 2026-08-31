#pragma once

#include <Common/Packet/PacketType.h>

namespace common::packet
{
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