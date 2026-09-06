#pragma once

#include <cstdint>

namespace common::game
{
	// 동시에 입력되는 이동 방향을 하나의 비트 플래그 값으로 표현
	enum class InputFlags : std::uint8_t
	{
		None = 0,
		Up = 1 << 0,
		Down = 1 << 1,
		Left = 1 << 2,
		Right = 1 << 3
	};

	[[nodiscard]] inline InputFlags operator|(InputFlags left, InputFlags right) noexcept
	{
		return static_cast<InputFlags>(static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
	}

	[[nodiscard]] inline InputFlags operator&(InputFlags left, InputFlags right) noexcept
	{
		return static_cast<InputFlags>(static_cast<std::uint8_t>(left) & static_cast<std::uint8_t>(right));
	}

	inline InputFlags& operator|=(InputFlags& left, InputFlags right) noexcept
	{
		left = left | right;
		return left;
	}

	[[nodiscard]] inline bool HasFlag(InputFlags flags, InputFlags flag) noexcept
	{
		return (flags & flag) != InputFlags::None;
	}
}