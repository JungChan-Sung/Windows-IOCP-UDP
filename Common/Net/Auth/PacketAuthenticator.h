#pragma once

#include <span>

#include <Common/Net/Auth/PacketAuthentication.h>
#include <Common/Net/SessionToken.h>

namespace common::net
{
	[[nodiscard]] bool ComputePacketAuthenticationTag(
		const SessionToken& sessionToken,
		std::span<const char> data,
		PacketAuthenticationTag& tag
	) noexcept;

	[[nodiscard]] bool VerifyPacketAuthenticationTag(
		const SessionToken& sessionToken,
		std::span<const char> data,
		const PacketAuthenticationTag& tag
	) noexcept;
}