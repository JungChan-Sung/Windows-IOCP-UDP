#pragma once

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
}