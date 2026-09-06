#pragma once

#include <cstddef>

namespace persistence::config
{
	// server_configs의 NVARCHAR 컬럼과 동일한 UTF-16 Code Unit 기준의 길이 제한
	inline constexpr std::size_t maxServerConfigKeyUtf16CodeUnitCount = 128;
	inline constexpr std::size_t maxServerConfigValueUtf16CodeUnitCount = 512;
}