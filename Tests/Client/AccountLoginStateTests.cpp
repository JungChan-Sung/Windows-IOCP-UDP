#include "AccountLoginStateTests.h"

#include <chrono>
#include <optional>
#include <string>
#include <utility>

#include <Common/Packet/Account/AccountPacket.h>

#include <Client/Net/AccountLoginState.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using AccountLoginState = client::net::AccountLoginState;
	using State = AccountLoginState::State;
	using TimePoint = AccountLoginState::TimePoint;

	void RunInitialStateTest(tests::DebugTestResult& result)
	{
		AccountLoginState loginState;

		const AccountLoginState::Snapshot snapshot = loginState.GetSnapshot();

		tests::Expect(
			result,
			snapshot.state == State::Idle,
			"AccountLoginState: initial state is idle"
		);

		tests::Expect(
			result,
			snapshot.requestId == common::packet::invalidAccountLoginRequestId,
			"AccountLoginState: initial request id is invalid"
		);

		tests::Expect(
			result,
			!loginState.TryBuildRequest(TimePoint{}).has_value(),
			"AccountLoginState: idle state does not build request"
		);
	}

	void RunBeginAndRetryTest(tests::DebugTestResult& result)
	{
		AccountLoginState loginState;

		const TimePoint beginTime{};
		const auto retryInterval = std::chrono::milliseconds(500);

		const AccountLoginState::RequestId requestId = loginState.Begin(
			"account",
			"password_hash",
			beginTime,
			retryInterval
		);

		tests::Expect(
			result,
			requestId != common::packet::invalidAccountLoginRequestId,
			"AccountLoginState: begin allocates request id"
		);

		const AccountLoginState::Snapshot waitingSnapshot = loginState.GetSnapshot();

		tests::Expect(
			result,
			waitingSnapshot.state == State::WaitingResponse,
			"AccountLoginState: begin waits for response"
		);

		const std::optional<common::packet::AccountLoginRequestPacket> firstRequest
			= loginState.TryBuildRequest(beginTime);

		tests::Expect(
			result,
			firstRequest.has_value(),
			"AccountLoginState: begin allows immediate request"
		);

		if (firstRequest.has_value())
		{
			tests::Expect(
				result,
				firstRequest->requestId == requestId,
				"AccountLoginState: first request id"
			);

			tests::Expect(
				result,
				firstRequest->loginName == "account",
				"AccountLoginState: first request login name"
			);

			tests::Expect(
				result,
				firstRequest->passwordHash == "password_hash",
				"AccountLoginState: first request password hash"
			);
		}

		const std::optional<common::packet::AccountLoginRequestPacket> earlyRequest
			= loginState.TryBuildRequest(
				beginTime + std::chrono::milliseconds(499)
			);

		tests::Expect(
			result,
			!earlyRequest.has_value(),
			"AccountLoginState: early retry rejected"
		);

		const std::optional<common::packet::AccountLoginRequestPacket> retryRequest
			= loginState.TryBuildRequest(beginTime + retryInterval);

		tests::Expect(
			result,
			retryRequest.has_value(),
			"AccountLoginState: retry interval allows request"
		);

		if (retryRequest.has_value())
		{
			tests::Expect(
				result,
				retryRequest->requestId == requestId,
				"AccountLoginState: retry preserves request id"
			);
		}
	}

	void RunStaleResponseTest(tests::DebugTestResult& result)
	{
		AccountLoginState loginState;

		const AccountLoginState::RequestId requestId = loginState.Begin(
			"account",
			"password_hash",
			TimePoint{},
			std::chrono::milliseconds(500)
		);

		common::packet::AccountLoginResponsePacket staleResponse{};
		staleResponse.requestId = requestId + 1;
		staleResponse.status = common::packet::AccountLoginResponseStatus::Succeeded;
		staleResponse.accountId = 1001;
		staleResponse.nickname = "nickname";

		tests::Expect(
			result,
			!loginState.ApplyResponse(staleResponse),
			"AccountLoginState: stale response rejected"
		);

		tests::Expect(
			result,
			loginState.GetSnapshot().state == State::WaitingResponse,
			"AccountLoginState: stale response keeps waiting state"
		);
	}

	void RunSuccessResponseTest(tests::DebugTestResult& result)
	{
		AccountLoginState loginState;

		const AccountLoginState::RequestId requestId = loginState.Begin(
			"account",
			"password_hash",
			TimePoint{},
			std::chrono::milliseconds(500)
		);

		common::packet::AccountLoginResponsePacket response{};
		response.requestId = requestId;
		response.status = common::packet::AccountLoginResponseStatus::Succeeded;
		response.accountId = 1001;
		response.nickname = "nickname";

		tests::Expect(
			result,
			loginState.ApplyResponse(response),
			"AccountLoginState: success response accepted"
		);

		const AccountLoginState::Snapshot snapshot = loginState.GetSnapshot();

		tests::Expect(
			result,
			snapshot.state == State::Succeeded,
			"AccountLoginState: success state"
		);

		tests::Expect(
			result,
			snapshot.responseStatus.has_value()
			&& *snapshot.responseStatus
			== common::packet::AccountLoginResponseStatus::Succeeded,
			"AccountLoginState: success status stored"
		);

		tests::Expect(
			result,
			snapshot.accountId == 1001,
			"AccountLoginState: account id stored"
		);

		tests::Expect(
			result,
			snapshot.nickname == "nickname",
			"AccountLoginState: nickname stored"
		);

		tests::Expect(
			result,
			!loginState.TryBuildRequest(TimePoint{} + std::chrono::seconds(10)).has_value(),
			"AccountLoginState: success stops retries"
		);
	}

	void RunFailureResponseTest(tests::DebugTestResult& result)
	{
		AccountLoginState loginState;

		const AccountLoginState::RequestId requestId = loginState.Begin(
			"account",
			"wrong_password_hash",
			TimePoint{},
			std::chrono::milliseconds(500)
		);

		common::packet::AccountLoginResponsePacket response{};
		response.requestId = requestId;
		response.status = common::packet::AccountLoginResponseStatus::InvalidCredentials;

		tests::Expect(
			result,
			loginState.ApplyResponse(response),
			"AccountLoginState: failure response accepted"
		);

		const AccountLoginState::Snapshot snapshot = loginState.GetSnapshot();

		tests::Expect(
			result,
			snapshot.state == State::Failed,
			"AccountLoginState: failure state"
		);

		tests::Expect(
			result,
			snapshot.responseStatus.has_value()
			&& *snapshot.responseStatus
			== common::packet::AccountLoginResponseStatus::InvalidCredentials,
			"AccountLoginState: failure status stored"
		);

		tests::Expect(
			result,
			snapshot.accountId == 0 && snapshot.nickname.empty(),
			"AccountLoginState: failure clears account data"
		);

		tests::Expect(
			result,
			!loginState.TryBuildRequest(TimePoint{} + std::chrono::seconds(10)).has_value(),
			"AccountLoginState: failure stops retries"
		);
	}

	void RunNewAttemptRejectsPreviousResponseTest(tests::DebugTestResult& result)
	{
		AccountLoginState loginState;

		const AccountLoginState::RequestId previousRequestId = loginState.Begin(
			"first_account",
			"first_hash",
			TimePoint{},
			std::chrono::milliseconds(500)
		);

		const AccountLoginState::RequestId currentRequestId = loginState.Begin(
			"second_account",
			"second_hash",
			TimePoint{},
			std::chrono::milliseconds(500)
		);

		tests::Expect(
			result,
			currentRequestId != previousRequestId,
			"AccountLoginState: new attempt allocates new request id"
		);

		common::packet::AccountLoginResponsePacket previousResponse{};
		previousResponse.requestId = previousRequestId;
		previousResponse.status = common::packet::AccountLoginResponseStatus::Succeeded;
		previousResponse.accountId = 1001;
		previousResponse.nickname = "previous";

		tests::Expect(
			result,
			!loginState.ApplyResponse(previousResponse),
			"AccountLoginState: previous attempt response rejected"
		);

		tests::Expect(
			result,
			loginState.GetSnapshot().requestId == currentRequestId,
			"AccountLoginState: current request remains active"
		);
	}

	void RunResetTest(tests::DebugTestResult& result)
	{
		AccountLoginState loginState;

		loginState.Begin(
			"account",
			"password_hash",
			TimePoint{},
			std::chrono::milliseconds(500)
		);

		loginState.Reset();

		const AccountLoginState::Snapshot snapshot = loginState.GetSnapshot();

		tests::Expect(
			result,
			snapshot.state == State::Idle,
			"AccountLoginState: reset returns to idle"
		);

		tests::Expect(
			result,
			snapshot.requestId == common::packet::invalidAccountLoginRequestId,
			"AccountLoginState: reset clears active request id"
		);

		tests::Expect(
			result,
			snapshot.loginName.empty(),
			"AccountLoginState: reset clears login name"
		);

		tests::Expect(
			result,
			!snapshot.responseStatus.has_value(),
			"AccountLoginState: reset clears response status"
		);

		tests::Expect(
			result,
			!loginState.TryBuildRequest(TimePoint{} + std::chrono::seconds(10)).has_value(),
			"AccountLoginState: reset stops requests"
		);
	}
}

namespace tests::client
{
	DebugTestResult RunAccountLoginStateTests()
	{
		DebugTestResult result{};

		RunInitialStateTest(result);
		RunBeginAndRetryTest(result);
		RunStaleResponseTest(result);
		RunSuccessResponseTest(result);
		RunFailureResponseTest(result);
		RunNewAttemptRejectsPreviousResponseTest(result);
		RunResetTest(result);

		return result;
	}
}