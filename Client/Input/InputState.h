#pragma once

#include <atomic>
#include <cstdint>

#include <Client/Input/InputSnapshot.h>

namespace client::input
{
	class InputState
	{
	private:
		static inline constexpr std::uint32_t fireActionMask = 1U << 0;
		static inline constexpr std::uint32_t decreaseInterpolationActionMask = 1U << 1;
		static inline constexpr std::uint32_t increaseInterpolationActionMask = 1U << 2;
		static inline constexpr std::uint32_t toggleInterpolationActionMask = 1U << 3;
		static inline constexpr std::uint32_t togglePredictionActionMask = 1U << 4;
		static inline constexpr std::uint32_t toggleReconciliationActionMask = 1U << 5;

	private:
		std::atomic<bool> isUpPressed_ = false;
		std::atomic<bool> isDownPressed_ = false;
		std::atomic<bool> isLeftPressed_ = false;
		std::atomic<bool> isRightPressed_ = false;

		std::atomic<bool> isRoom1Pressed_ = false;
		std::atomic<bool> isRoom2Pressed_ = false;
		std::atomic<bool> isRoom3Pressed_ = false;

		std::atomic<std::uint32_t> pendingActionMask_ = 0;

	public:
		InputState() = default;
		~InputState() noexcept = default;

		InputState(const InputState&) = delete;
		InputState& operator=(const InputState&) = delete;

		InputState(InputState&&) = delete;
		InputState& operator=(InputState&&) = delete;

	public:
		void SetKeyDown(unsigned int virtualKey, bool isRepeat) noexcept;
		void SetKeyUp(unsigned int virtualKey) noexcept;
		void Clear() noexcept;

		[[nodiscard]] InputSnapshot ConsumeSnapshot() noexcept;
	};
}