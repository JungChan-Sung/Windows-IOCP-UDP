#pragma once

#include <cstdint>

namespace common::packet
{
	enum class PacketType : std::uint16_t
	{
		None = 0,

		JoinRequest = 1,
		JoinResponse = 2,

		InputCommand = 3,
		FireRequest = 4,

		LeaveRequest = 5,

		JoinRoomRequest = 6,
		JoinRoomResponse = 7,

		PlayerJoined = 8,
		PlayerLeft = 9,

		PlayerSnapshot = 10,
		BulletSnapshot = 11,
		ImpactEffect = 12,

		AccountLoginRequest = 13,
		AccountLoginResponse = 14,

		LeaveResponse = 15,

		KeepAlive = 16,

		Count,
	};
}