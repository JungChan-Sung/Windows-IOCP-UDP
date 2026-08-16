#include "InvalidPacketLogLimiterTests.h"

#include <chrono>
#include <cstdint>

#include <Server/Diagnostics/InvalidPacketLogLimiter.h>
#include <Server/Protocol/UdpPacketDispatcher.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using DispatchStatus =
		server::protocol::UdpPacketDispatcher::DispatchStatus;

	using Clock =
		server::diagnostics::InvalidPacketLogLimiter::Clock;

	using TimePoint =
		server::diagnostics::InvalidPacketLogLimiter::TimePoint;

	void RunInitialStateTest(tests::DebugTestResult& result)
	{
		server::diagnostics::InvalidPacketLogLimiter logLimiter;

		tests::Expect(
			result,
			logLimiter.GetTotalDroppedCount() == 0,
			"InvalidPacketLogLimiter: initial total dropped count"
		);
	}

	void RunSucceededStatusIgnoredTest(tests::DebugTestResult& result)
	{
		server::diagnostics::InvalidPacketLogLimiter logLimiter;
		const TimePoint now = Clock::now();

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision decision =
			logLimiter.Record(
				DispatchStatus::Succeeded,
				now
			);

		tests::Expect(
			result,
			!decision.shouldLog,
			"InvalidPacketLogLimiter: succeeded status does not log"
		);

		tests::Expect(
			result,
			decision.totalCount == 0,
			"InvalidPacketLogLimiter: succeeded status total count"
		);

		tests::Expect(
			result,
			decision.suppressedCount == 0,
			"InvalidPacketLogLimiter: succeeded status suppressed count"
		);

		tests::Expect(
			result,
			logLimiter.GetTotalDroppedCount() == 0,
			"InvalidPacketLogLimiter: succeeded status not counted"
		);
	}

	void RunCountStatusIgnoredTest(tests::DebugTestResult& result)
	{
		server::diagnostics::InvalidPacketLogLimiter logLimiter;
		const TimePoint now = Clock::now();

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision decision =
			logLimiter.Record(
				DispatchStatus::Count,
				now
			);

		tests::Expect(
			result,
			!decision.shouldLog,
			"InvalidPacketLogLimiter: count status does not log"
		);

		tests::Expect(
			result,
			decision.totalCount == 0,
			"InvalidPacketLogLimiter: count status total count"
		);

		tests::Expect(
			result,
			decision.suppressedCount == 0,
			"InvalidPacketLogLimiter: count status suppressed count"
		);

		tests::Expect(
			result,
			logLimiter.GetTotalDroppedCount() == 0,
			"InvalidPacketLogLimiter: count status not counted"
		);
	}

	void RunImmediateLogLimitTest(tests::DebugTestResult& result)
	{
		server::diagnostics::InvalidPacketLogLimiter logLimiter;
		const TimePoint now = Clock::now();

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision
			firstDecision =
			logLimiter.Record(
				DispatchStatus::InvalidPacketHeader,
				now
			);

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision
			secondDecision =
			logLimiter.Record(
				DispatchStatus::InvalidPacketHeader,
				now
			);

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision
			thirdDecision =
			logLimiter.Record(
				DispatchStatus::InvalidPacketHeader,
				now
			);

		tests::Expect(
			result,
			firstDecision.shouldLog,
			"InvalidPacketLogLimiter: first invalid packet logs"
		);

		tests::Expect(
			result,
			secondDecision.shouldLog,
			"InvalidPacketLogLimiter: second invalid packet logs"
		);

		tests::Expect(
			result,
			thirdDecision.shouldLog,
			"InvalidPacketLogLimiter: third invalid packet logs"
		);

		tests::Expect(
			result,
			firstDecision.totalCount == 1,
			"InvalidPacketLogLimiter: first total count"
		);

		tests::Expect(
			result,
			secondDecision.totalCount == 2,
			"InvalidPacketLogLimiter: second total count"
		);

		tests::Expect(
			result,
			thirdDecision.totalCount == 3,
			"InvalidPacketLogLimiter: third total count"
		);

		tests::Expect(
			result,
			logLimiter.GetTotalDroppedCount() == 3,
			"InvalidPacketLogLimiter: immediate total dropped count"
		);
	}

	void RunSuppressesAfterImmediateLimitTest(
		tests::DebugTestResult& result
	)
	{
		server::diagnostics::InvalidPacketLogLimiter logLimiter;
		const TimePoint now = Clock::now();

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision
			fourthDecision =
			logLimiter.Record(
				DispatchStatus::InvalidPacketHeader,
				now
			);

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision
			fifthDecision =
			logLimiter.Record(
				DispatchStatus::InvalidPacketHeader,
				now
			);

		tests::Expect(
			result,
			!fourthDecision.shouldLog,
			"InvalidPacketLogLimiter: fourth invalid packet suppressed"
		);

		tests::Expect(
			result,
			!fifthDecision.shouldLog,
			"InvalidPacketLogLimiter: fifth invalid packet suppressed"
		);

		tests::Expect(
			result,
			fourthDecision.totalCount == 4,
			"InvalidPacketLogLimiter: fourth total count"
		);

		tests::Expect(
			result,
			fifthDecision.totalCount == 5,
			"InvalidPacketLogLimiter: fifth total count"
		);

		tests::Expect(
			result,
			fourthDecision.suppressedCount == 1,
			"InvalidPacketLogLimiter: fourth suppressed count"
		);

		tests::Expect(
			result,
			fifthDecision.suppressedCount == 2,
			"InvalidPacketLogLimiter: fifth suppressed count"
		);

		tests::Expect(
			result,
			logLimiter.GetTotalDroppedCount() == 5,
			"InvalidPacketLogLimiter: suppressed total dropped count"
		);
	}

	void RunLogsAgainAfterIntervalTest(tests::DebugTestResult& result)
	{
		server::diagnostics::InvalidPacketLogLimiter logLimiter;
		const TimePoint now = Clock::now();

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision
			intervalDecision =
			logLimiter.Record(
				DispatchStatus::InvalidPacketHeader,
				now + std::chrono::seconds(5)
			);

		tests::Expect(
			result,
			intervalDecision.shouldLog,
			"InvalidPacketLogLimiter: interval invalid packet logs"
		);

		tests::Expect(
			result,
			intervalDecision.totalCount == 6,
			"InvalidPacketLogLimiter: interval total count"
		);

		tests::Expect(
			result,
			intervalDecision.suppressedCount == 2,
			"InvalidPacketLogLimiter: interval reports suppressed count"
		);

		tests::Expect(
			result,
			logLimiter.GetTotalDroppedCount() == 6,
			"InvalidPacketLogLimiter: interval total dropped count"
		);

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision
			nextDecision =
			logLimiter.Record(
				DispatchStatus::InvalidPacketHeader,
				now + std::chrono::seconds(5)
			);

		tests::Expect(
			result,
			!nextDecision.shouldLog,
			"InvalidPacketLogLimiter: next packet after interval is suppressed again"
		);

		tests::Expect(
			result,
			nextDecision.suppressedCount == 1,
			"InvalidPacketLogLimiter: suppressed count reset after interval log"
		);
	}

	void RunDifferentStatusHasIndependentStateTest(
		tests::DebugTestResult& result
	)
	{
		server::diagnostics::InvalidPacketLogLimiter logLimiter;
		const TimePoint now = Clock::now();

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision
			otherStatusDecision =
			logLimiter.Record(
				DispatchStatus::InvalidPacketPayload,
				now
			);

		tests::Expect(
			result,
			otherStatusDecision.shouldLog,
			"InvalidPacketLogLimiter: different status logs independently"
		);

		tests::Expect(
			result,
			otherStatusDecision.totalCount == 1,
			"InvalidPacketLogLimiter: different status total count"
		);

		tests::Expect(
			result,
			otherStatusDecision.suppressedCount == 0,
			"InvalidPacketLogLimiter: different status suppressed count"
		);

		tests::Expect(
			result,
			logLimiter.GetTotalDroppedCount() == 5,
			"InvalidPacketLogLimiter: different status total dropped count"
		);
	}

	void RunResetTest(tests::DebugTestResult& result)
	{
		server::diagnostics::InvalidPacketLogLimiter logLimiter;
		const TimePoint now = Clock::now();

		logLimiter.Record(
			DispatchStatus::InvalidPacketHeader,
			now
		);

		logLimiter.Record(
			DispatchStatus::InvalidPacketPayload,
			now
		);

		tests::Expect(
			result,
			logLimiter.GetTotalDroppedCount() == 2,
			"InvalidPacketLogLimiter: total dropped count before reset"
		);

		logLimiter.Reset();

		tests::Expect(
			result,
			logLimiter.GetTotalDroppedCount() == 0,
			"InvalidPacketLogLimiter: total dropped count after reset"
		);

		const server::diagnostics::InvalidPacketLogLimiter::LogDecision decision =
			logLimiter.Record(
				DispatchStatus::InvalidPacketHeader,
				now
			);

		tests::Expect(
			result,
			decision.shouldLog,
			"InvalidPacketLogLimiter: first invalid packet logs after reset"
		);

		tests::Expect(
			result,
			decision.totalCount == 1,
			"InvalidPacketLogLimiter: total count after reset"
		);

		tests::Expect(
			result,
			decision.suppressedCount == 0,
			"InvalidPacketLogLimiter: suppressed count after reset"
		);
	}
}

namespace tests::server
{
	tests::DebugTestResult RunInvalidPacketLogLimiterTests()
	{
		tests::DebugTestResult result{};

		RunInitialStateTest(result);
		RunSucceededStatusIgnoredTest(result);
		RunCountStatusIgnoredTest(result);
		RunImmediateLogLimitTest(result);
		RunSuppressesAfterImmediateLimitTest(result);
		RunLogsAgainAfterIntervalTest(result);
		RunDifferentStatusHasIndependentStateTest(result);
		RunResetTest(result);

		return result;
	}
}