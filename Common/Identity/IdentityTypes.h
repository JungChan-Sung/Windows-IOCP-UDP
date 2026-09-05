#pragma once

#include <cstdint>

namespace common::identity
{
	// DB에 저장되어 세션 수명을 넘어 유지되는 영속성 식별자 타입을 정의
	using AccountId = std::int64_t;
	using PersistentPlayerId = std::int64_t;
	using MatchId = std::int64_t;
}