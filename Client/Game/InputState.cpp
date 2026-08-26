#include "InputState.h"

#include <Windows.h>

namespace client::game
{
	void InputState::SetKeyDown(unsigned int virtualKey) noexcept
	{
		switch (virtualKey)
		{
		case 'W':
		case VK_UP:
			isUpPressed_.store(true, std::memory_order_relaxed);
			break;

		case 'S':
		case VK_DOWN:
			isDownPressed_.store(true, std::memory_order_relaxed);
			break;

		case 'A':
		case VK_LEFT:
			isLeftPressed_.store(true, std::memory_order_relaxed);
			break;

		case 'D':
		case VK_RIGHT:
			isRightPressed_.store(true, std::memory_order_relaxed);
			break;

		default:
			break;
		}
	}

	void InputState::SetKeyUp(unsigned int virtualKey) noexcept
	{
		switch (virtualKey)
		{
		case 'W':
		case VK_UP:
			isUpPressed_.store(false, std::memory_order_relaxed);
			break;

		case 'S':
		case VK_DOWN:
			isDownPressed_.store(false, std::memory_order_relaxed);
			break;

		case 'A':
		case VK_LEFT:
			isLeftPressed_.store(false, std::memory_order_relaxed);
			break;

		case 'D':
		case VK_RIGHT:
			isRightPressed_.store(false, std::memory_order_relaxed);
			break;

		default:
			break;
		}
	}

	void InputState::Clear() noexcept
	{
		isUpPressed_.store(false, std::memory_order_relaxed);
		isDownPressed_.store(false, std::memory_order_relaxed);
		isLeftPressed_.store(false, std::memory_order_relaxed);
		isRightPressed_.store(false, std::memory_order_relaxed);
	}

	common::game::InputFlags InputState::ToInputFlags() const noexcept
	{
		common::game::InputFlags inputFlags = common::game::InputFlags::None;

		if (isUpPressed_.load(std::memory_order_relaxed))
		{
			inputFlags |= common::game::InputFlags::Up;
		}

		if (isDownPressed_.load(std::memory_order_relaxed))
		{
			inputFlags |= common::game::InputFlags::Down;
		}

		if (isLeftPressed_.load(std::memory_order_relaxed))
		{
			inputFlags |= common::game::InputFlags::Left;
		}

		if (isRightPressed_.load(std::memory_order_relaxed))
		{
			inputFlags |= common::game::InputFlags::Right;
		}

		return inputFlags;
	}
}