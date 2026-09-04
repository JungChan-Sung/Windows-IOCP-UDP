#pragma once

namespace common::packet
{
	// 패킷 타입별 Wire 형식과 직렬화 규칙을 특수화 템플릿 인터페이스
	template <typename TPacket>
	struct PacketCodec;

	template <typename TPacket>
	inline constexpr int packetExpectedSize = PacketCodec<TPacket>::fixedWireSize;
}