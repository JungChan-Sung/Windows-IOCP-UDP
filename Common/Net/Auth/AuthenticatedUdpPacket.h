#pragma once

#include <cstddef>
#include <optional>

#include <Common/Net/Auth/PacketAuthentication.h>
#include <Common/Net/SessionToken.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/PacketHeader.h>

namespace common::net
{
	inline constexpr std::size_t packetAuthenticationSequenceWireSize = packet::uint32WireSize;
	// 인증 Sequence와 태그를 패킷 끝에 추가하는 Trailer의 Wire 크기
	inline constexpr std::size_t authenticatedUdpPacketTrailerWireSize = packetAuthenticationSequenceWireSize + packetAuthenticationTagSize;

	// 인증 정보를 파싱한 비소유 뷰 구조체
	// packetData는 원본 패킷 메모리를 참조
	struct AuthenticatedUdpPacketView
	{
	public:
		packet::PacketHeader packetHeader{};
		PacketAuthentication authentication{};

		packet::ConstPacketSpan packetData;
	};

	//┌──────────────┐
	//│PacketHeader         6 Byte │
	//├──────────────┤
	//│기존 Packet Body            │
	//│                            │
	//│Reliable 패킷이라면         │
	//│ReliableHeader도 여기에 있음│
	//├──────────────┤
	//│Auth Sequence        4 Byte │
	//├──────────────┤
	//│HMAC - SHA256 Tag    32 Byte│
	//└──────────────┘
	// Total : 36 byte
	// 기존 전송 속성을 유지하면서 인증 Sequence와 HMAC 태그를 추가하는 함수
	[[nodiscard]] std::optional<packet::PacketBuffer> BuildAuthenticatedUdpPacket(
		const SessionToken& sessionToken,
		PacketAuthenticationSequence sequence,
		packet::ConstPacketSpan serializedPacket
	);

	[[nodiscard]] std::optional<AuthenticatedUdpPacketView> ParseAuthenticatedUdpPacket(const char* packetData, int packetSize) noexcept;

	[[nodiscard]] bool VerifyAuthenticatedUdpPacket(const SessionToken& sessionToken, const AuthenticatedUdpPacketView& packetView) noexcept;

	// 인증 Trailer와 Auth flag를 제거해 하위 전송 계층 처리 전의 패킷을 복원하는 함수
	[[nodiscard]] std::optional<packet::PacketBuffer> BuildUnauthenticatedUdpPacket(const AuthenticatedUdpPacketView& packetView);
}