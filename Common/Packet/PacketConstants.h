#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace common::packet
{
	inline constexpr std::uint8_t uint8WireSize = 1;
	inline constexpr std::uint8_t uint16WireSize = 2;
	inline constexpr std::uint8_t uint32WireSize = 4;
	inline constexpr std::uint8_t uint64WireSize = 8;
	inline constexpr std::uint8_t int32WireSize = 4;
	inline constexpr std::uint8_t int64WireSize = 8;
	inline constexpr std::uint8_t floatWireSize = 4;

	inline constexpr std::size_t stringLengthWireSize = uint16WireSize;

	inline constexpr std::uint16_t protocolVersion = 5;

	//  15          14              13                  0
	//	┌─────┬───────┬─────────┐
	//	│ Reliable │ Authenticated│ Protocol Version │
	//	│ flag     │ flag         │                  │
	//	└─────┴───────┴─────────┘
	// Version 필드 상위 비트 : 전송 속성(Reliable, Authenticated) 표시
	// 나머지 비트 : 실제 프로토콜 버전
	inline constexpr std::uint16_t packetHeaderAuthenticatedFlag = 0x4000;
	inline constexpr std::uint16_t packetHeaderReliableFlag = 0x8000;
	inline constexpr std::uint16_t packetHeaderVersionMask = 0x3FFF;

	inline constexpr std::size_t packetTypeWireSize = uint16WireSize;

	inline constexpr std::size_t serializedPacketHeaderSize
		= uint16WireSize
		+ packetTypeWireSize
		+ uint16WireSize;
	inline constexpr std::size_t maxSerializedPacketSize = 1200;	// UDP 전송 버퍼와 공유하는 단일 직렬화 패킷 최대 크기

	[[nodiscard]] inline constexpr std::size_t GetSerializedStringSize(std::string_view value) noexcept
	{
		return stringLengthWireSize + value.size();
	}
}