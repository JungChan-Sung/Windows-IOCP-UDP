#pragma once

#include <WinSock2.h>

#include <Common/Net/EndpointKey.h>

namespace common::net
{
	[[nodiscard]] inline EndpointKey MakeEndpointKey(const sockaddr_in& address) noexcept
	{
		EndpointKey key{};
		key.address = address.sin_addr.S_un.S_addr;
		key.port = ::ntohs(address.sin_port);
		return key;
	}

	[[nodiscard]] inline sockaddr_in MakeSocketAddress(const EndpointKey& endpointKey) noexcept
	{
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.S_un.S_addr = endpointKey.address;
		address.sin_port = ::htons(endpointKey.port);
		return address;
	}
}