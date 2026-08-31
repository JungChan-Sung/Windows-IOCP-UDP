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
	inline constexpr std::size_t authenticatedUdpPacketTrailerWireSize = packetAuthenticationSequenceWireSize + packetAuthenticationTagSize;

	struct AuthenticatedUdpPacketView
	{
	public:
		packet::PacketHeader packetHeader{};
		PacketAuthentication authentication{};

		packet::ConstPacketSpan packetData;
	};

	[[nodiscard]] std::optional<packet::PacketBuffer> BuildAuthenticatedUdpPacket(
		const SessionToken& sessionToken,
		PacketAuthenticationSequence sequence,
		packet::ConstPacketSpan serializedPacket
	);

	[[nodiscard]] std::optional<AuthenticatedUdpPacketView> ParseAuthenticatedUdpPacket(const char* packetData, int packetSize) noexcept;

	[[nodiscard]] bool VerifyAuthenticatedUdpPacket(const SessionToken& sessionToken, const AuthenticatedUdpPacketView& packetView) noexcept;

	[[nodiscard]] std::optional<packet::PacketBuffer> BuildUnauthenticatedUdpPacket(const AuthenticatedUdpPacketView& packetView);
}