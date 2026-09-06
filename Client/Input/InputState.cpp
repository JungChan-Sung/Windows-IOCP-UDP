#include "InputState.h"

#include <Windows.h>

namespace client::input
{
	void InputState::SetKeyDown(unsigned int virtualKey, bool isRepeat) noexcept
	{
		switch (virtualKey)
		{
		case 'W':
		case VK_UP:
			isUpPressed_.store(true, std::memory_order_relaxed);
			return;

		case 'S':
		case VK_DOWN:
			isDownPressed_.store(true, std::memory_order_relaxed);
			return;

		case 'A':
		case VK_LEFT:
			isLeftPressed_.store(true, std::memory_order_relaxed);
			return;

		case 'D':
		case VK_RIGHT:
			isRightPressed_.store(true, std::memory_order_relaxed);
			return;

		case '1':
			isRoom1Pressed_.store(true, std::memory_order_relaxed);
			return;

		case '2':
			isRoom2Pressed_.store(true, std::memory_order_relaxed);
			return;

		case '3':
			isRoom3Pressed_.store(true, std::memory_order_relaxed);
			return;

		default:
			break;
		}

		if (isRepeat)
		{
			return;
		}

		switch (virtualKey)
		{
		case VK_SPACE:
			pendingActionMask_.fetch_or(fireActionMask, std::memory_order_relaxed);
			break;

		case 'I':
			pendingActionMask_.fetch_or(toggleInterpolationActionMask, std::memory_order_relaxed);
			break;

		case VK_OEM_MINUS:
		case VK_SUBTRACT:
			pendingActionMask_.fetch_or(decreaseInterpolationActionMask, std::memory_order_relaxed);
			break;

		case VK_OEM_PLUS:
		case VK_ADD:
			pendingActionMask_.fetch_or(increaseInterpolationActionMask, std::memory_order_relaxed);
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

		case '1':
			isRoom1Pressed_.store(false, std::memory_order_relaxed);
			break;

		case '2':
			isRoom2Pressed_.store(false, std::memory_order_relaxed);
			break;

		case '3':
			isRoom3Pressed_.store(false, std::memory_order_relaxed);
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

		isRoom1Pressed_.store(false, std::memory_order_relaxed);
		isRoom2Pressed_.store(false, std::memory_order_relaxed);
		isRoom3Pressed_.store(false, std::memory_order_relaxed);

		pendingActionMask_.store(0, std::memory_order_relaxed);
	}

	InputSnapshot InputState::ConsumeSnapshot() noexcept
	{
		InputSnapshot snapshot{};

		if (isUpPressed_.load(std::memory_order_relaxed))
		{
			snapshot.movementFlags |= common::game::InputFlags::Up;
		}

		if (isDownPressed_.load(std::memory_order_relaxed))
		{
			snapshot.movementFlags |= common::game::InputFlags::Down;
		}

		if (isLeftPressed_.load(std::memory_order_relaxed))
		{
			snapshot.movementFlags |= common::game::InputFlags::Left;
		}

		if (isRightPressed_.load(std::memory_order_relaxed))
		{
			snapshot.movementFlags |= common::game::InputFlags::Right;
		}

		snapshot.isRoom1Pressed = isRoom1Pressed_.load(std::memory_order_relaxed);
		snapshot.isRoom2Pressed = isRoom2Pressed_.load(std::memory_order_relaxed);
		snapshot.isRoom3Pressed = isRoom3Pressed_.load(std::memory_order_relaxed);

		const std::uint32_t actionMask = pendingActionMask_.exchange(0, std::memory_order_relaxed);

		snapshot.fireRequested = (actionMask & fireActionMask) != 0;

		snapshot.toggleInterpolationRequested = (actionMask & toggleInterpolationActionMask) != 0;
snapshot.decreaseInterpolationRequested = (actionMask & decreaseInterpolationActionMask) != 0;
		snapshot.increaseInterpolationRequested = (actionMask & increaseInterpolationActionMask) != 0;

		return snapshot;
	}
}