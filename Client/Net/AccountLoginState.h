#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Time/TimeTypes.h>

namespace client::net
{
	class AccountLoginState final
	{
	public:
		using RequestId = common::packet::AccountLoginRequestId;
		using ResponseStatus = common::packet::AccountLoginResponseStatus;

	public:
		enum class State : std::uint8_t
		{
			Idle = 0,
			WaitingResponse,
			Succeeded,
			Failed,
		};

		struct Snapshot
		{
		public:
			State state = State::Idle;
			RequestId requestId = common::packet::invalidAccountLoginRequestId;

			std::string loginName;

			std::optional<ResponseStatus> responseStatus;

			std::int64_t accountId = 0;
			std::string nickname;
		};

	public:
		using Clock = common::time::Clock;
		using TimePoint = common::time::TimePoint;
		using Duration = common::time::Duration;

	private:
		mutable std::mutex mutex_;

		RequestId nextRequestId_ = 1;
		RequestId requestId_ = common::packet::invalidAccountLoginRequestId;

		State state_ = State::Idle;

		Duration retryInterval_;
		TimePoint nextAttemptTime_;

		std::string loginName_;
		std::string passwordHash_;

		std::optional<ResponseStatus> responseStatus_;

		std::int64_t accountId_ = 0;
		std::string nickname_;

	public:
		AccountLoginState();
		~AccountLoginState() noexcept = default;

		AccountLoginState(const AccountLoginState&) = delete;
		AccountLoginState& operator=(const AccountLoginState&) = delete;

		AccountLoginState(AccountLoginState&&) = delete;
		AccountLoginState& operator=(AccountLoginState&&) = delete;

	public:
		[[nodiscard]] RequestId Begin(std::string loginName, std::string passwordHash, TimePoint currentTime, Duration retryInterval);

		[[nodiscard]] std::optional<common::packet::AccountLoginRequestPacket> TryBuildRequest(TimePoint currentTime);

		[[nodiscard]] bool ApplyResponse(const common::packet::AccountLoginResponsePacket& packet);

		void Reset();

	private:
		[[nodiscard]] RequestId AllocateRequestIdUnlocked() noexcept;
		void ClearResultUnlocked();

	public:
		[[nodiscard]] Snapshot GetSnapshot() const;
	};
}