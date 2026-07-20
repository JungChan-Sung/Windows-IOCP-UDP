#pragma once

#include <cstddef>

namespace persistence::account
{
	inline constexpr std::size_t maxLoginNameUtf16CodeUnitCount = 50;
	inline constexpr std::size_t maxPasswordHashUtf16CodeUnitCount = 255;
	inline constexpr std::size_t maxNicknameUtf16CodeUnitCount = 50;
}