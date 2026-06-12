#pragma once

#include <chrono>
#include <cstdint>

namespace client::net
{
	class JoinHandshakeState
	{
	public:
		enum class State : std::uint8_t
		{
			Idle = 0,
			WaitingResponse,
			Completed,
		};

	public:
		using Clock = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;
		using Duration = std::chrono::milliseconds;

	private:
		State state_ = State::Idle;
		Duration retryInterval_{};
		TimePoint nextAttemptTime_{};

	public:
		JoinHandshakeState() = default;
		~JoinHandshakeState() noexcept = default;

		JoinHandshakeState(const JoinHandshakeState&) = delete;
		JoinHandshakeState& operator=(const JoinHandshakeState&) = delete;

		JoinHandshakeState(JoinHandshakeState&&) = delete;
		JoinHandshakeState& operator=(JoinHandshakeState&&) = delete;

	public:
		void Begin(TimePoint currentTime, Duration retryInterval) noexcept
		{
			state_ = State::WaitingResponse;
			retryInterval_ = retryInterval;
			nextAttemptTime_ = currentTime;
		}

		[[nodiscard]] bool TryStartAttempt(TimePoint currentTime) noexcept
		{
			if (state_ != State::WaitingResponse || currentTime < nextAttemptTime_)
			{
				return false;
			}

			nextAttemptTime_ = currentTime + retryInterval_;
			return true;
		}

		void Complete() noexcept
		{
			if (state_ == State::WaitingResponse)
			{
				state_ = State::Completed;
			}
		}

		void Reset() noexcept
		{
			state_ = State::Idle;
			retryInterval_ = {};
			nextAttemptTime_ = {};
		}

	public:
		[[nodiscard]] State GetState() const noexcept
		{
			return state_;
		}
	};
}