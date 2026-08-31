#pragma once

#include <Common/Net/SequenceNumber.h>
#include <Common/Net/SessionToken.h>

namespace common::net
{
	using PacketAuthenticationSequence = SequenceNumber;

	struct PacketAuthentication
	{
	public:
		SessionToken sessionToken{};
		PacketAuthenticationSequence sequence = 0;
	};
}