#pragma once

#include <array>
#include <cstddef>

#include <Common/Packet/PacketType.h>

namespace common::packet
{
	enum class PacketReliability
	{
		Unreliable,
		Reliable,
	};

	inline constexpr auto packetReliabilityTable = std::to_array<PacketReliability>({
		PacketReliability::Unreliable,	// None

		PacketReliability::Unreliable,	// JoinRequest
		PacketReliability::Unreliable,	// JoinResponse

		PacketReliability::Unreliable,	// InputCommand
		PacketReliability::Unreliable,	// FireRequest

		PacketReliability::Reliable,	// LeaveRequest

		PacketReliability::Reliable,	// JoinRoomRequest
		PacketReliability::Reliable,	// JoinRoomResponse

		PacketReliability::Unreliable,	// PlayerJoined
		PacketReliability::Unreliable,	// PlayerLeft

		PacketReliability::Unreliable,	// PlayerSnapshot
		PacketReliability::Unreliable,	// BulletSnapshot
		PacketReliability::Unreliable,	// ImpactEffect

		PacketReliability::Unreliable,	// AccountLoginRequest
		PacketReliability::Unreliable,	// AccountLoginResponse

		PacketReliability::Reliable,	// LeaveResponse

		PacketReliability::Unreliable,	// KeepAlive
		});

	static_assert(packetReliabilityTable.size() == static_cast<std::size_t>(PacketType::Count));

	[[nodiscard]] inline PacketReliability GetPacketReliability(PacketType packetType) noexcept
	{
		const std::size_t index = static_cast<std::size_t>(packetType);
		if (index >= packetReliabilityTable.size())
		{
			return PacketReliability::Unreliable;
		}

		return packetReliabilityTable[index];
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