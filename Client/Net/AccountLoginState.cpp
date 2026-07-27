#include "AccountLoginState.h"

#include <utility>

namespace client::net
{
	AccountLoginState::AccountLoginState()
		: retryInterval_(common::time::Milliseconds(0)),
		nextAttemptTime_(common::time::Milliseconds(0))
	{}

	AccountLoginState::RequestId AccountLoginState::Begin(std::string loginName, std::string passwordHash, TimePoint currentTime, Duration retryInterval)
	{
		std::scoped_lock lock(mutex_);

		requestId_ = AllocateRequestIdUnlocked();

		state_ = State::WaitingResponse;

		retryInterval_ = (retryInterval < Duration::zero()) ? Duration::zero() : retryInterval;

		nextAttemptTime_ = currentTime;

		loginName_ = std::move(loginName);
		passwordHash_ = std::move(passwordHash);

		ClearResultUnlocked();

		return requestId_;
	}

	std::optional<common::packet::AccountLoginRequestPacket> AccountLoginState::TryBuildRequest(TimePoint currentTime)
	{
		std::scoped_lock lock(mutex_);

		if (state_ != State::WaitingResponse)
		{
			return std::nullopt;
		}

		if (currentTime < nextAttemptTime_)
		{
			return std::nullopt;
		}

		nextAttemptTime_ = currentTime + retryInterval_;

		return common::packet::AccountLoginRequestPacket{
			.requestId = requestId_,
			.loginName = loginName_,
			.passwordHash = passwordHash_,
		};
	}

	bool AccountLoginState::ApplyResponse(const common::packet::AccountLoginResponsePacket& packet)
	{
		std::scoped_lock lock(mutex_);

		if (state_ != State::WaitingResponse)
		{
			return false;
		}

		if (packet.requestId != requestId_)
		{
			return false;
		}

		responseStatus_ = packet.status;

		passwordHash_.clear();

		if (packet.status == ResponseStatus::Succeeded)
		{
			state_ = State::Succeeded;
			accountId_ = packet.accountId;
			nickname_ = packet.nickname;

			return true;
		}

		state_ = State::Failed;
		accountId_ = 0;
		nickname_.clear();

		return true;
	}

	void AccountLoginState::Reset()
	{
		std::scoped_lock lock(mutex_);

		requestId_ = common::packet::invalidAccountLoginRequestId;
		state_ = State::Idle;

		retryInterval_ = {};
		nextAttemptTime_ = {};

		loginName_.clear();
		passwordHash_.clear();

		ClearResultUnlocked();
	}

	AccountLoginState::RequestId AccountLoginState::AllocateRequestIdUnlocked() noexcept
	{
		const RequestId requestId = nextRequestId_;

		++nextRequestId_;
		if (nextRequestId_ == common::packet::invalidAccountLoginRequestId)
		{
			nextRequestId_ = 1;
		}

		return requestId;
	}

	void AccountLoginState::ClearResultUnlocked()
	{
		responseStatus_.reset();

		accountId_ = 0;
		nickname_.clear();
	}

	AccountLoginState::Snapshot AccountLoginState::GetSnapshot() const
	{
		std::scoped_lock lock(mutex_);

		return Snapshot{
			.state = state_,
			.requestId = requestId_,
			.loginName = loginName_,
			.responseStatus = responseStatus_,
			.accountId = accountId_,
			.nickname = nickname_,
		};
	}
}