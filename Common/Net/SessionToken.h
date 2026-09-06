#pragma once

#include <cstdint>

namespace common::net
{
	// 세션 식별과 패킷 인증에 사용하는 128비트 토큰 값 구조체
	struct SessionToken
	{
	public:
		std::uint64_t high = 0;
		std::uint64_t low = 0;

	public:
		bool operator==(const SessionToken& other) const = default;
	};

	inline constexpr SessionToken invalidSessionToken{}; // 모든 비트가 0인 토큰을 유효하지 않은 세션 값으로 예약

	[[nodiscard]] inline bool IsValidSessionToken(const SessionToken& sessionToken) noexcept
	{
		return sessionToken != invalidSessionToken;
	}
}