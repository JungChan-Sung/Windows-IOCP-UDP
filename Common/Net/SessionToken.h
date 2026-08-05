#pragma once

#include <cstdint>

namespace common::net
{
	struct SessionToken
	{
	public:
		std::uint64_t high = 0;
		std::uint64_t low = 0;

	public:
		bool operator==(const SessionToken& other) const = default;
	};

	inline constexpr SessionToken invalidSessionToken{};

	[[nodiscard]] inline bool IsValidSessionToken(const SessionToken& sessionToken) noexcept
	{
		return sessionToken != invalidSessionToken;
	}
}