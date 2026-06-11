#include "ReliableUdpSendWindowTests.h"

#include <chrono>
#include <cstdint>

#include <Common/Net/ReliableUdpSession.h>
#include <Common/Packet/PacketBuffer.h>

#include <Tests/DebugTestResult.h>

namespace
{
	[[nodiscard]] common::packet::PacketBuffer MakePacketBuffer(char value)
	{
		return common::packet::PacketBuffer{ value };
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

	void RunProcessReceivedDataHeaderUpdatesAckTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		common::net::ReliableUdpPacketHeader receivedHeader{};
		receivedHeader.sequence = 10;
		receivedHeader.ackSequence = 0;
		receivedHeader.ackBitfield = 0;

		const bool isNewReliablePacket = session.ProcessReceivedDataHeader(receivedHeader);

		tests::Expect(result, isNewReliablePacket, "ReliableUdpSession: received data header is new");
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

		const bool firstReceiveResult = session.ProcessReceivedDataHeader(firstReceivedHeader);
		const bool secondReceiveResult = session.ProcessReceivedDataHeader(secondReceivedHeader);

		tests::Expect(result, firstReceiveResult, "ReliableUdpSession: first received header is new");
		tests::Expect(result, secondReceiveResult, "ReliableUdpSession: second received header is new");

		const common::net::ReliableSequence outgoingSequence = session.AllocateOutgoingSequence();
		const common::net::ReliableUdpPacketHeader outgoingHeader = session.BuildOutgoingHeader(outgoingSequence);

		tests::Expect(result, outgoingHeader.sequence == 1, "ReliableUdpSession: outgoing sequence after received headers");
		tests::Expect(result, outgoingHeader.ackSequence == 11, "ReliableUdpSession: outgoing ack sequence");
		tests::Expect(result, outgoingHeader.ackBitfield == 1, "ReliableUdpSession: outgoing ack bitfield");
	}

	void RunProcessReceivedDataHeaderRemovesAckedPendingPacketTest(tests::DebugTestResult& result)
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

		const bool isNewReliablePacket = session.ProcessReceivedDataHeader(receivedHeader);

		tests::Expect(result, isNewReliablePacket, "ReliableUdpSession: received ack header is new");
		tests::Expect(result, session.GetPendingPacketCount() == 1, "ReliableUdpSession: pending count after ack");
	}

	void RunProcessReceivedAckRemovesPendingPacketTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		const common::net::ReliableUdpSession::TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		const common::net::ReliableSequence sequence = session.AllocateOutgoingSequence();
		const bool registerResult = session.RegisterSentPacket(sequence, MakePacketBuffer('A'), currentTime);

		tests::Expect(result, registerResult, "ReliableUdpSession: sent packet registered");
		tests::Expect(result, session.GetPendingPacketCount() == 1, "ReliableUdpSession: pending count before ack-only");

		common::net::ReliableUdpPacketHeader ackHeader{};
		ackHeader.ackSequence = sequence;
		ackHeader.ackBitfield = 0;

		const bool ackProcessed = session.ProcessReceivedAck(ackHeader);

		tests::Expect(result, ackProcessed, "ReliableUdpSession: ack-only processed");
		tests::Expect(result, session.GetPendingPacketCount() == 0, "ReliableUdpSession: pending count after ack-only");
		tests::Expect(result, !session.HasReceivedAnySequence(), "ReliableUdpSession: ack-only does not update received sequence");
	}

	void RunProcessReceivedAckRejectsFutureAckTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		const common::net::ReliableUdpSession::TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		const common::net::ReliableSequence sequence = session.AllocateOutgoingSequence();
		const bool registerResult = session.RegisterSentPacket(sequence, MakePacketBuffer('A'), currentTime);

		tests::Expect(result, registerResult, "ReliableUdpSession: sent packet registered before future ack");
		tests::Expect(result, session.GetPendingPacketCount() == 1, "ReliableUdpSession: pending count before future ack");

		common::net::ReliableUdpPacketHeader ackHeader{};
		ackHeader.ackSequence = sequence + 10;
		ackHeader.ackBitfield = 0xFFFFFFFF;

		const bool ackProcessed = session.ProcessReceivedAck(ackHeader);

		tests::Expect(result, !ackProcessed, "ReliableUdpSession: future ack rejected");
		tests::Expect(result, session.GetPendingPacketCount() == 1, "ReliableUdpSession: future ack keeps pending packet");
		tests::Expect(result, !session.HasReceivedAnySequence(), "ReliableUdpSession: rejected ack-only does not update received sequence");
	}

	void RunExtractResendPacketsTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;
		session.SetResendInterval(std::chrono::milliseconds(100));

		const common::net::ReliableUdpSession::TimePoint startTime = common::net::ReliableUdpSession::Clock::now();

		const common::net::ReliableSequence sequence = session.AllocateOutgoingSequence();
		const bool registerResult = session.RegisterSentPacket(sequence, MakePacketBuffer('A'), startTime);

		tests::Expect(result, registerResult, "ReliableUdpSession: sent packet registered before resend test");

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

	void RunExtractResendResultGiveUpTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;
		session.SetMaxResendCount(1);
		session.SetResendInterval(std::chrono::milliseconds(100));

		const common::net::ReliableUdpSession::TimePoint startTime = common::net::ReliableUdpSession::Clock::now();

		const common::net::ReliableSequence sequence = session.AllocateOutgoingSequence();
		const bool registerResult = session.RegisterSentPacket(sequence, MakePacketBuffer('A'), startTime);

		tests::Expect(result, registerResult, "ReliableUdpSession: sent packet registered before give-up test");

		const common::net::ReliableUdpSession::ResendResult firstResult =
			session.ExtractResendResult(startTime + std::chrono::milliseconds(100));

		tests::Expect(result, firstResult.resendPacketList.size() == 1, "ReliableUdpSession: first timeout resends packet");
		tests::Expect(result, firstResult.giveUpPacketList.empty(), "ReliableUdpSession: first timeout does not give up packet");

		if (firstResult.resendPacketList.size() == 1)
		{
			tests::Expect(result, firstResult.resendPacketList[0].sequence == sequence, "ReliableUdpSession: resend sequence before give-up");
			tests::Expect(result, firstResult.resendPacketList[0].resendCount == 1, "ReliableUdpSession: resend count before give-up");
		}

		tests::Expect(result, session.GetPendingPacketCount() == 1, "ReliableUdpSession: resent packet remains pending");

		const common::net::ReliableUdpSession::ResendResult secondResult =
			session.ExtractResendResult(startTime + std::chrono::milliseconds(200));

		tests::Expect(result, secondResult.resendPacketList.empty(), "ReliableUdpSession: give-up timeout does not resend packet");
		tests::Expect(result, secondResult.giveUpPacketList.size() == 1, "ReliableUdpSession: give-up timeout extracts packet");

		if (secondResult.giveUpPacketList.size() == 1)
		{
			tests::Expect(result, secondResult.giveUpPacketList[0].sequence == sequence, "ReliableUdpSession: give-up sequence");
			tests::Expect(result, secondResult.giveUpPacketList[0].resendCount == 1, "ReliableUdpSession: give-up resend count");
		}

		tests::Expect(result, session.GetPendingPacketCount() == 0, "ReliableUdpSession: give-up removes pending packet");
	}

	void RunResetTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		const common::net::ReliableUdpSession::TimePoint currentTime = common::net::ReliableUdpSession::Clock::now();

		const common::net::ReliableSequence sequence = session.AllocateOutgoingSequence();
		const bool registerResult = session.RegisterSentPacket(sequence, MakePacketBuffer('A'), currentTime);

		tests::Expect(result, registerResult, "ReliableUdpSession: sent packet registered before reset");

		common::net::ReliableUdpPacketHeader receivedHeader{};
		receivedHeader.sequence = 10;
		const bool isNewReliablePacket = session.ProcessReceivedDataHeader(receivedHeader);

		tests::Expect(result, isNewReliablePacket, "ReliableUdpSession: received packet before reset");

		session.Reset();

		tests::Expect(result, session.GetNextSequence() == 1, "ReliableUdpSession: reset next sequence");
		tests::Expect(result, session.GetPendingPacketCount() == 0, "ReliableUdpSession: reset pending count");
		tests::Expect(result, !session.HasReceivedAnySequence(), "ReliableUdpSession: reset ack tracker");
	}
}

namespace tests::net
{
	tests::DebugTestResult RunReliableUdpSendWindowTests()
	{
		tests::DebugTestResult result{};

		RunBuildOutgoingHeaderWithoutAckTest(result);
		RunProcessReceivedDataHeaderUpdatesAckTest(result);
		RunBuildOutgoingHeaderWithAckTest(result);
		RunProcessReceivedDataHeaderRemovesAckedPendingPacketTest(result);
		RunProcessReceivedAckRemovesPendingPacketTest(result);
		RunProcessReceivedAckRejectsFutureAckTest(result);
		RunExtractResendPacketsTest(result);
		RunExtractResendResultGiveUpTest(result);
		RunResetTest(result);

		return result;
	}
}