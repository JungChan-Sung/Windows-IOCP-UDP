#pragma once

#include <cstdint>
#include <functional>

namespace common::net
{
	//63                              16 15              0
	//┌────────────────┬───────┐
	//│         IPv4 Address           │     Port     │
	//│          32 bits               │   16 bits    │
	//└────────────────┴───────┘
	// IPv4 주소와 UDP 포트를 조합해 통신 상대를 식별하는 값 타입 구조체
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