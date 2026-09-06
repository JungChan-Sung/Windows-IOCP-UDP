#pragma once

#include <cstddef>

namespace persistence::account
{
	// SQL Server NVARCHAR 컬럼 크기와 동일한 UTF-16 Code Unit 기준의 입력 길이 제한
	inline constexpr std::size_t maxLoginNameUtf16CodeUnitCount = 50;
	inline constexpr std::size_t maxPasswordHashUtf16CodeUnitCount = 255;
	inline constexpr std::size_t maxNicknameUtf16CodeUnitCount = 50;
}