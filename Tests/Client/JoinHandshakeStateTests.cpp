#include "JoinHandshakeStateTests.h"

#include <chrono>

#include <Client/Net/JoinHandshakeState.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using JoinHandshakeState = client::net::JoinHandshakeState;
	using State = JoinHandshakeState::State;
	using TimePoint = JoinHandshakeState::TimePoint;

	void RunInitialStateTest(tests::DebugTestResult& result)
	{
		JoinHandshakeState handshakeState;

		tests::Expect(
			result,
			handshakeState.GetState() == State::Idle,
			"JoinHandshakeState: initial state is idle"
		);

		tests::Expect(
			result,
			!handshakeState.TryStartAttempt(TimePoint{}),
			"JoinHandshakeState: idle state cannot start attempt"
		);
	}

	void RunBeginAllowsImmediateAttemptTest(tests::DebugTestResult& result)
	{
		JoinHandshakeState handshakeState;

		const TimePoint beginTime{};
		handshakeState.Begin(beginTime, std::chrono::milliseconds(500));

		tests::Expect(
			result,
			handshakeState.GetState() == State::WaitingResponse,
			"JoinHandshakeState: begin waits for response"
		);

		tests::Expect(
			result,
			handshakeState.TryStartAttempt(beginTime),
			"JoinHandshakeState: begin allows immediate attempt"
		);
	}

	void RunRetryIntervalTest(tests::DebugTestResult& result)
	{
		JoinHandshakeState handshakeState;

		const TimePoint beginTime{};
		const auto retryInterval = std::chrono::milliseconds(500);

		handshakeState.Begin(beginTime, retryInterval);

		const bool firstAttemptResult = handshakeState.TryStartAttempt(beginTime);
		const bool earlyAttemptResult =
			handshakeState.TryStartAttempt(beginTime + std::chrono::milliseconds(499));
		const bool retryAttemptResult =
			handshakeState.TryStartAttempt(beginTime + retryInterval);

		tests::Expect(
			result,
			firstAttemptResult,
			"JoinHandshakeState: first attempt succeeds"
		);

		tests::Expect(
			result,
			!earlyAttemptResult,
			"JoinHandshakeState: early retry rejected"
		);

		tests::Expect(
			result,
			retryAttemptResult,
			"JoinHandshakeState: retry interval allows attempt"
		);
	}

	void RunCompleteStopsAttemptsTest(tests::DebugTestResult& result)
	{
		JoinHandshakeState handshakeState;

		const TimePoint beginTime{};
		handshakeState.Begin(beginTime, std::chrono::milliseconds(500));
		handshakeState.TryStartAttempt(beginTime);
		handshakeState.Complete();

		tests::Expect(
			result,
			handshakeState.GetState() == State::Completed,
			"JoinHandshakeState: complete changes state"
		);

		tests::Expect(
			result,
			!handshakeState.TryStartAttempt(beginTime + std::chrono::seconds(10)),
			"JoinHandshakeState: completed state stops attempts"
		);
	}

	void RunResetTest(tests::DebugTestResult& result)
	{
		JoinHandshakeState handshakeState;

		const TimePoint beginTime{};
		handshakeState.Begin(beginTime, std::chrono::milliseconds(500));
		handshakeState.Complete();
		handshakeState.Reset();

		tests::Expect(
			result,
			handshakeState.GetState() == State::Idle,
			"JoinHandshakeState: reset returns to idle"
		);

		tests::Expect(
			result,
			!handshakeState.TryStartAttempt(beginTime + std::chrono::seconds(10)),
			"JoinHandshakeState: reset does not allow attempt"
		);
	}
}

namespace tests::client
{
	tests::DebugTestResult RunJoinHandshakeStateTests()
	{
		tests::DebugTestResult result{};

		RunInitialStateTest(result);
		RunBeginAllowsImmediateAttemptTest(result);
		RunRetryIntervalTest(result);
		RunCompleteStopsAttemptsTest(result);
		RunResetTest(result);

		return result;
	}
}