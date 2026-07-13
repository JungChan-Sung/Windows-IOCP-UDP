#include "ReliableUdpProtocolTests.h"

#include <cstdint>
#include <limits>

#include <Common/Net/Reliable/ReliableUdpProtocol.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void RunSequenceComparisonTests(tests::DebugTestResult& result)
	{
		using common::net::ReliableSequence;

		tests::Expect(result, common::net::IsSequenceNewer(2, 1), "ReliableUdpProtocol: 2 newer than 1");
		tests::Expect(result, !common::net::IsSequenceNewer(1, 2), "ReliableUdpProtocol: 1 not newer than 2");
		tests::Expect(result, !common::net::IsSequenceNewer(10, 10), "ReliableUdpProtocol: same sequence not newer");

		const ReliableSequence maxSequence = std::numeric_limits<ReliableSequence>::max();

		tests::Expect(result, common::net::IsSequenceNewer(0, maxSequence), "ReliableUdpProtocol: wrap 0 newer than max");
		tests::Expect(result, common::net::IsSequenceOlder(maxSequence, 0), "ReliableUdpProtocol: max older than wrap 0");
	}

	void RunAckBitfieldFunctionTests(tests::DebugTestResult& result)
	{
		const std::uint32_t ackBitfield =
			(static_cast<std::uint32_t>(1) << 0)
			| (static_cast<std::uint32_t>(1) << 2);

		tests::Expect(result, common::net::IsSequenceAcked(100, 100, ackBitfield), "ReliableUdpProtocol: ack sequence acked");
		tests::Expect(result, common::net::IsSequenceAcked(99, 100, ackBitfield), "ReliableUdpProtocol: ack bit 0 acked");
		tests::Expect(result, !common::net::IsSequenceAcked(98, 100, ackBitfield), "ReliableUdpProtocol: missing ack bit not acked");
		tests::Expect(result, common::net::IsSequenceAcked(97, 100, ackBitfield), "ReliableUdpProtocol: ack bit 2 acked");
		tests::Expect(result, !common::net::IsSequenceAcked(101, 100, ackBitfield), "ReliableUdpProtocol: future sequence not acked");
		tests::Expect(result, !common::net::IsSequenceAcked(67, 100, ackBitfield), "ReliableUdpProtocol: too old sequence not acked");
	}

	void RunAckTrackerSequentialTests(tests::DebugTestResult& result)
	{
		common::net::ReliableAckTracker ackTracker;

		tests::Expect(result, !ackTracker.HasReceivedAnySequence(), "ReliableAckTracker: initially empty");
		tests::Expect(result, !ackTracker.IsSequenceAcked(10), "ReliableAckTracker: empty tracker acks nothing");

		ackTracker.ObserveReceivedSequence(10);

		tests::Expect(result, ackTracker.HasReceivedAnySequence(), "ReliableAckTracker: has first sequence");
		tests::Expect(result, ackTracker.GetLatestReceivedSequence() == 10, "ReliableAckTracker: first latest sequence");
		tests::Expect(result, ackTracker.GetAckBitfield() == 0, "ReliableAckTracker: first ack bitfield empty");
		tests::Expect(result, ackTracker.IsSequenceAcked(10), "ReliableAckTracker: first sequence acked");
		tests::Expect(result, !ackTracker.IsSequenceAcked(9), "ReliableAckTracker: previous sequence missing");

		ackTracker.ObserveReceivedSequence(11);

		tests::Expect(result, ackTracker.GetLatestReceivedSequence() == 11, "ReliableAckTracker: second latest sequence");
		tests::Expect(result, ackTracker.IsSequenceAcked(11), "ReliableAckTracker: latest sequence acked");
		tests::Expect(result, ackTracker.IsSequenceAcked(10), "ReliableAckTracker: previous sequence acked by bitfield");
	}

	void RunAckTrackerOutOfOrderTests(tests::DebugTestResult& result)
	{
		common::net::ReliableAckTracker ackTracker;

		ackTracker.ObserveReceivedSequence(10);
		ackTracker.ObserveReceivedSequence(11);
		ackTracker.ObserveReceivedSequence(13);

		tests::Expect(result, ackTracker.GetLatestReceivedSequence() == 13, "ReliableAckTracker: out of order latest sequence");
		tests::Expect(result, ackTracker.IsSequenceAcked(13), "ReliableAckTracker: sequence 13 acked");
		tests::Expect(result, !ackTracker.IsSequenceAcked(12), "ReliableAckTracker: missing sequence 12 not acked");
		tests::Expect(result, ackTracker.IsSequenceAcked(11), "ReliableAckTracker: sequence 11 acked");
		tests::Expect(result, ackTracker.IsSequenceAcked(10), "ReliableAckTracker: sequence 10 acked");

		ackTracker.ObserveReceivedSequence(12);

		tests::Expect(result, ackTracker.IsSequenceAcked(12), "ReliableAckTracker: late sequence 12 acked");
	}

	void RunAckTrackerTooOldTests(tests::DebugTestResult& result)
	{
		common::net::ReliableAckTracker ackTracker;

		ackTracker.ObserveReceivedSequence(100);
		ackTracker.ObserveReceivedSequence(68);
		ackTracker.ObserveReceivedSequence(67);

		tests::Expect(result, ackTracker.IsSequenceAcked(100), "ReliableAckTracker: latest sequence 100 acked");
		tests::Expect(result, ackTracker.IsSequenceAcked(68), "ReliableAckTracker: distance 32 sequence acked");
		tests::Expect(result, !ackTracker.IsSequenceAcked(67), "ReliableAckTracker: distance 33 sequence ignored");
	}

	void RunAckTrackerWrapTests(tests::DebugTestResult& result)
	{
		using common::net::ReliableSequence;

		const ReliableSequence maxSequence = std::numeric_limits<ReliableSequence>::max();

		common::net::ReliableAckTracker ackTracker;

		ackTracker.ObserveReceivedSequence(maxSequence - 1);
		ackTracker.ObserveReceivedSequence(maxSequence);
		ackTracker.ObserveReceivedSequence(0);

		tests::Expect(result, ackTracker.GetLatestReceivedSequence() == 0, "ReliableAckTracker: wrap latest sequence");
		tests::Expect(result, ackTracker.IsSequenceAcked(0), "ReliableAckTracker: wrapped sequence 0 acked");
		tests::Expect(result, ackTracker.IsSequenceAcked(maxSequence), "ReliableAckTracker: max sequence acked after wrap");
		tests::Expect(result, ackTracker.IsSequenceAcked(maxSequence - 1), "ReliableAckTracker: max - 1 sequence acked after wrap");
	}
}

namespace tests::net
{
	tests::DebugTestResult RunReliableUdpProtocolTests()
	{
		tests::DebugTestResult result{};

		RunSequenceComparisonTests(result);
		RunAckBitfieldFunctionTests(result);
		RunAckTrackerSequentialTests(result);
		RunAckTrackerOutOfOrderTests(result);
		RunAckTrackerTooOldTests(result);
		RunAckTrackerWrapTests(result);

		return result;
	}
}