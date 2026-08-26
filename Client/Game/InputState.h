#pragma once

#include <atomic>

#include <Common/Game/InputFlags.h>

namespace client::game
{
	class InputState
	{
	private:
		std::atomic<bool> isUpPressed_ = false;
		std::atomic<bool> isDownPressed_ = false;
		std::atomic<bool> isLeftPressed_ = false;
		std::atomic<bool> isRightPressed_ = false;

	public:
		InputState() = default;
		~InputState() noexcept = default;

		InputState(const InputState&) = delete;
		InputState& operator=(const InputState&) = delete;

		InputState(InputState&&) = delete;
		InputState& operator=(InputState&&) = delete;

	public:
		void SetKeyDown(unsigned int virtualKey) noexcept;
		void SetKeyUp(unsigned int virtualKey) noexcept;
		void Clear() noexcept;

		[[nodiscard]] common::game::InputFlags ToInputFlags() const noexcept;
	};
}