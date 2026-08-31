#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <Common/Net/SequenceNumber.h>

namespace common::net
{
	using PacketAuthenticationSequence = SequenceNumber;

	inline constexpr std::size_t packetAuthenticationTagSize = 32;

	using PacketAuthenticationTag = std::array<std::uint8_t, packetAuthenticationTagSize>;

	struct PacketAuthentication
	{
	public:
		PacketAuthenticationSequence sequence = 0;
		PacketAuthenticationTag tag{};
	};
}