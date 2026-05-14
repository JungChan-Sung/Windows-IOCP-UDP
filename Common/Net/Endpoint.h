#pragma once

#include <WinSock2.h>

#include <cstdint>
#include <functional>

namespace common::net
{
	struct EndpointKey
	{
	public:
		std::uint32_t address = 0;
		std::uint16_t port = 0;

	public:
		bool operator==(const EndpointKey& other) const = default;
	};

	struct EndpointKeyHasher
	{
		[[nodiscard]] std::size_t operator()(const EndpointKey& key) const noexcept
		{
			const std::uint64_t combined = (static_cast<std::uint64_t>(key.address) << 16) | key.port;

			return std::hash<std::uint64_t>{}(combined);
		}
	};

	[[nodiscard]] inline EndpointKey MakeEndpointKey(const sockaddr_in& address) noexcept
	{
		EndpointKey key{};
		key.address = address.sin_addr.S_un.S_addr;
		key.port = ::ntohs(address.sin_port);
		return key;
	}
}