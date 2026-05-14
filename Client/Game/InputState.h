#pragma once

#include <Common/Game/InputFlags.h>

namespace client::game
{
	class InputState
	{
	private:
		bool isUpPressed_ = false;
		bool isDownPressed_ = false;
		bool isLeftPressed_ = false;
		bool isRightPressed_ = false;

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

	public:
		[[nodiscard]] common::game::InputFlags ToInputFlags() const noexcept;
	};
}

