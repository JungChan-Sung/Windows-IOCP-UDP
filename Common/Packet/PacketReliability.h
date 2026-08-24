#pragma once

#include <Common/Packet/PacketType.h>

namespace common::packet
{
	enum class PacketReliability
	{
		Unreliable,
		Reliable,
	};

	[[nodiscard]] inline PacketReliability GetPacketReliability(PacketType packetType) noexcept
	{
		switch (packetType)
		{
		case PacketType::JoinRoomRequest:
		case PacketType::JoinRoomResponse:
		case PacketType::LeaveRequest:
			return PacketReliability::Reliable;

		default:
			return PacketReliability::Unreliable;
		}
	}

	[[nodiscard]] inline bool IsReliablePacketType(PacketType packetType) noexcept
	{
		return GetPacketReliability(packetType) == PacketReliability::Reliable;
	}

	[[nodiscard]] inline bool IsPacketTransportReliabilityValid(PacketType packetType, bool isReliable) noexcept
	{
		if (packetType == PacketType::None)
		{
			return isReliable;
		}

		return IsReliablePacketType(packetType) == isReliable;
	}
}