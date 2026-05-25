#include "ReliableUdpSessionTests.h"

#include <chrono>
#include <vector>

#include <Common/Net/ReliableUdpSession.h>

#include <Tests/DebugTestResult.h>

namespace tests::net::reliableUdpSessionTest
{
	[[nodiscard]] std::vector<char> MakePacketBuffer(char value)
	{
		return std::vector<char>{ value };
	}

	void RunBuildOutgoingHeaderWithoutAckTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		const common::net::ReliableSequence sequence = session.AllocateOutgoingSequence();
		const common::net::ReliableUdpPacketHeader header = session.BuildOutgoingHeader(sequence);

		tests::Expect(result, sequence == 1, "ReliableUdpSession: first outgoing sequence");
		tests::Expect(result, header.sequence == 1, "ReliableUdpSession: outgoing header sequence");
		tests::Expect(result, header.ackSequence == 0, "ReliableUdpSession: outgoing header default ack sequence");
		tests::Expect(result, header.ackBitfield == 0, "ReliableUdpSession: outgoing header default ack bitfield");
	}

	void RunProcessReceivedHeaderUpdatesAckTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		common::net::ReliableUdpPacketHeader receivedHeader{};
		receivedHeader.sequence = 10;
		receivedHeader.ackSequence = 0;
		receivedHeader.ackBitfield = 0;

		session.ProcessReceivedHeader(receivedHeader);

		tests::Expect(result, session.HasReceivedAnySequence(), "ReliableUdpSession: received any sequence");
		tests::Expect(result, session.GetLatestReceivedSequence() == 10, "ReliableUdpSession: latest received sequence");
		tests::Expect(result, session.GetAckBitfield() == 0, "ReliableUdpSession: first received ack bitfield");
		tests::Expect(result, session.HasReceivedSequence(10), "ReliableUdpSession: received sequence acked");
	}

	void RunBuildOutgoingHeaderWithAckTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		common::net::ReliableUdpPacketHeader firstReceivedHeader{};
		firstReceivedHeader.sequence = 10;

		common::net::ReliableUdpPacketHeader secondReceivedHeader{};
		secondReceivedHeader.sequence = 11;

		session.ProcessReceivedHeader(firstReceivedHeader);
		session.ProcessReceivedHeader(secondReceivedHeader);

		const common::net::ReliableSequence outgoingSequence = session.AllocateOutgoingSequence();
		const common::net::ReliableUdpPacketHeader outgoingHeader = session.BuildOutgoingHeader(outgoingSequence);

		tests::Expect(result, outgoingHeader.sequence == 1, "ReliableUdpSession: outgoing sequence after received headers");
		tests::Expect(result, outgoingHeader.ackSequence == 11, "ReliableUdpSession: outgoing ack sequence");
		tests::Expect(result, outgoingHeader.ackBitfield == 1, "ReliableUdpSession: outgoing ack bitfield");
	}

	void RunProcessReceivedHeaderRemovesAckedPendingPacketTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		const common::net::ReliableUdpSession::TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		const common::net::ReliableSequence firstSequence = session.AllocateOutgoingSequence();
		const bool firstRegisterResult = session.RegisterSentPacket(firstSequence, MakePacketBuffer('A'), currentTime);

		const common::net::ReliableSequence secondSequence = session.AllocateOutgoingSequence();
		const bool secondRegisterResult = session.RegisterSentPacket(secondSequence, MakePacketBuffer('B'), currentTime);

		tests::Expect(result, firstRegisterResult, "ReliableUdpSession: first sent packet registered");
		tests::Expect(result, secondRegisterResult, "ReliableUdpSession: second sent packet registered");
		tests::Expect(result, session.GetPendingPacketCount() == 2, "ReliableUdpSession: pending count before ack");

		common::net::ReliableUdpPacketHeader receivedHeader{};
		receivedHeader.sequence = 100;
		receivedHeader.ackSequence = firstSequence;
		receivedHeader.ackBitfield = 0;

		session.ProcessReceivedHeader(receivedHeader);

		tests::Expect(result, session.GetPendingPacketCount() == 1, "ReliableUdpSession: pending count after ack");
	}

	void RunExtractResendPacketsTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;
		session.SetResendInterval(std::chrono::milliseconds(100));

		const common::net::ReliableUdpSession::TimePoint startTime = common::net::ReliableUdpSession::Clock::now();

		const common::net::ReliableSequence sequence = session.AllocateOutgoingSequence();
		session.RegisterSentPacket(sequence, MakePacketBuffer('A'), startTime);

		const common::net::ReliableUdpSession::ResendPacketList earlyResendList =
			session.ExtractResendPackets(startTime + std::chrono::milliseconds(50));

		tests::Expect(result, earlyResendList.empty(), "ReliableUdpSession: early resend empty");

		const common::net::ReliableUdpSession::ResendPacketList resendList =
			session.ExtractResendPackets(startTime + std::chrono::milliseconds(100));

		tests::Expect(result, resendList.size() == 1, "ReliableUdpSession: resend count");

		if (resendList.size() == 1)
		{
			tests::Expect(result, resendList[0].sequence == sequence, "ReliableUdpSession: resend sequence");
			tests::Expect(result, resendList[0].resendCount == 1, "ReliableUdpSession: resend count value");
		}
	}

	void RunResetTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		const common::net::ReliableUdpSession::TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		const common::net::ReliableSequence sequence = session.AllocateOutgoingSequence();
		session.RegisterSentPacket(sequence, MakePacketBuffer('A'), currentTime);

		common::net::ReliableUdpPacketHeader receivedHeader{};
		receivedHeader.sequence = 10;
		session.ProcessReceivedHeader(receivedHeader);

		session.Reset();

		tests::Expect(result, session.GetNextSequence() == 1, "ReliableUdpSession: reset next sequence");
		tests::Expect(result, session.GetPendingPacketCount() == 0, "ReliableUdpSession: reset pending count");
		tests::Expect(result, !session.HasReceivedAnySequence(), "ReliableUdpSession: reset ack tracker");
	}
}

namespace tests::net
{
	tests::DebugTestResult RunReliableUdpSessionTests()
	{
		tests::DebugTestResult result{};

		reliableUdpSessionTest::RunBuildOutgoingHeaderWithoutAckTest(result);
		reliableUdpSessionTest::RunProcessReceivedHeaderUpdatesAckTest(result);
		reliableUdpSessionTest::RunBuildOutgoingHeaderWithAckTest(result);
		reliableUdpSessionTest::RunProcessReceivedHeaderRemovesAckedPendingPacketTest(result);
		reliableUdpSessionTest::RunExtractResendPacketsTest(result);
		reliableUdpSessionTest::RunResetTest(result);

		return result;
	}
}