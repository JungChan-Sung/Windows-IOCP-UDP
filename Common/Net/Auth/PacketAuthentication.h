#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <Common/Net/SequenceNumber.h>

namespace common::net
{
	using PacketAuthenticationSequence = SequenceNumber;

	// HMAC-SHA256 인증 태그의 크기
	inline constexpr std::size_t packetAuthenticationTagSize = 32;

	using PacketAuthenticationTag = std::array<std::uint8_t, packetAuthenticationTagSize>;

	// 패킷 인증 태그와 Replay 방지를 위한 Sequence를 함께 보관하는 구조체
	struct PacketAuthentication
	{
	public:
		PacketAuthenticationSequence sequence = 0;
		PacketAuthenticationTag tag{};
	};
}