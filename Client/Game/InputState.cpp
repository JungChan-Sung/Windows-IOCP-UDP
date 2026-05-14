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
			isUpPressed_ = true;
			break;

		case 'S':
		case VK_DOWN:
			isDownPressed_ = true;
			break;

		case 'A':
		case VK_LEFT:
			isLeftPressed_ = true;
			break;

		case 'D':
		case VK_RIGHT:
			isRightPressed_ = true;
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
			isUpPressed_ = false;
			break;

		case 'S':
		case VK_DOWN:
			isDownPressed_ = false;
			break;

		case 'A':
		case VK_LEFT:
			isLeftPressed_ = false;
			break;

		case 'D':
		case VK_RIGHT:
			isRightPressed_ = false;
			break;

		default:
			break;
		}
	}

	void InputState::Clear() noexcept
	{
		isUpPressed_ = false;
		isDownPressed_ = false;
		isLeftPressed_ = false;
		isRightPressed_ = false;
	}

	common::game::InputFlags InputState::ToInputFlags() const noexcept
	{
		common::game::InputFlags inputFlags = common::game::InputFlags::None;

		if (isUpPressed_)
		{
			inputFlags |= common::game::InputFlags::Up;
		}

		if (isDownPressed_)
		{
			inputFlags |= common::game::InputFlags::Down;
		}

		if (isLeftPressed_)
		{
			inputFlags |= common::game::InputFlags::Left;
		}

		if (isRightPressed_)
		{
			inputFlags |= common::game::InputFlags::Right;
		}

		return inputFlags;
	}
}