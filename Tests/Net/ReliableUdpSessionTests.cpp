#include "ReliableUdpSessionTests.h"

#include <chrono>
#include <vector>

#include <Common/Net/Reliable/ReliableUdpConfig.h>
#include <Common/Net/Reliable/ReliableUdpSession.h>
#include <Common/Net/Reliable/ReliableUdpPacketBuilder.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Packet/PacketSerialization.h>

#include <Tests/DebugTestResult.h>

namespace tests::net::reliableUdpSessionTest
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

		session.ProcessReceivedDataHeader(firstReceivedHeader);
		session.ProcessReceivedDataHeader(secondReceivedHeader);

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

	void RunExtractResendPacketsTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;
		session.SetResendInterval(common::time::Milliseconds(100));

		const common::net::ReliableUdpSession::TimePoint startTime = common::net::ReliableUdpSession::Clock::now();

		const common::net::ReliableSequence sequence = session.AllocateOutgoingSequence();
		session.RegisterSentPacket(sequence, MakePacketBuffer('A'), startTime);

		const common::net::ReliableUdpSession::ResendPacketList earlyResendList =
			session.ExtractResendPackets(startTime + common::time::Milliseconds(50));

		tests::Expect(result, earlyResendList.empty(), "ReliableUdpSession: early resend empty");

		const common::net::ReliableUdpSession::ResendPacketList resendList =
			session.ExtractResendPackets(startTime + common::time::Milliseconds(100));

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
		session.ProcessReceivedDataHeader(receivedHeader);

		session.Reset();

		tests::Expect(result, session.GetNextSequence() == 1, "ReliableUdpSession: reset next sequence");
		tests::Expect(result, session.GetPendingPacketCount() == 0, "ReliableUdpSession: reset pending count");
		tests::Expect(result, !session.HasReceivedAnySequence(), "ReliableUdpSession: reset ack tracker");
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

		session.ProcessReceivedAck(ackHeader);

		tests::Expect(result, session.GetPendingPacketCount() == 0, "ReliableUdpSession: pending count after ack-only");
		tests::Expect(result, !session.HasReceivedAnySequence(), "ReliableUdpSession: ack-only does not update received sequence");
	}

	void RunConfigureTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		common::net::ReliableUdpConfig config{};
		config.maxPendingPacketCount = 3;
		config.maxResendCount = 1;
		config.resendInterval = common::time::Milliseconds(150);

		session.Configure(config);

		const common::net::ReliableUdpSession::TimePoint now = common::net::ReliableUdpSession::Clock::now();

		const bool firstRegistered = session.RegisterSentPacket(session.AllocateOutgoingSequence(), MakePacketBuffer('A'), now);
		const bool secondRegistered = session.RegisterSentPacket(session.AllocateOutgoingSequence(), MakePacketBuffer('B'), now);
		const bool thirdRegistered = session.RegisterSentPacket(session.AllocateOutgoingSequence(), MakePacketBuffer('C'), now);
		const bool fourthRegistered = session.RegisterSentPacket(session.AllocateOutgoingSequence(), MakePacketBuffer('D'), now);

		tests::Expect(result, firstRegistered && secondRegistered && thirdRegistered, "ReliableUdpSession: configured pending packets accepted");
		tests::Expect(result, !fourthRegistered, "ReliableUdpSession: configured max pending packet count");
		tests::Expect(result, session.GetMaxResendCount() == 1, "ReliableUdpSession: configured max resend count");

		const common::net::ReliableUdpSession::ResendResult earlyResult = session.ExtractResendResult(now + common::time::Milliseconds(149));
		tests::Expect(result, earlyResult.resendPacketList.empty(), "ReliableUdpSession: configured resend interval blocks early resend");

		const common::net::ReliableUdpSession::ResendResult resendResult = session.ExtractResendResult(now + common::time::Milliseconds(150));
		tests::Expect(result, resendResult.resendPacketList.size() == 3, "ReliableUdpSession: configured resend interval");
	}

	void RunBuildOutgoingPacketTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSession session;

		const common::packet::JoinRoomResponsePacket packet{
			.roomId = 1,
			.spawnX = 10.0F,
			.spawnY = 20.0F,
		};

		const std::optional<common::packet::PacketBuffer> packetBuffer = common::packet::SerializePacket(packet);
		tests::Expect(result, packetBuffer.has_value(), "ReliableUdpSession: outgoing game packet serialized");

		if (!packetBuffer.has_value())
		{
			return;
		}

		const common::time::TimePoint currentTime = common::time::Clock::now();

		const common::net::ReliableUdpSession::BuildOutgoingPacketResult buildResult =
			session.BuildOutgoingPacket(common::packet::ConstPacketSpan(packetBuffer->data(), packetBuffer->size()), currentTime);

		tests::Expect(result, buildResult.has_value(), "ReliableUdpSession: outgoing reliable packet built");
		tests::Expect(result, session.GetPendingPacketCount() == 1, "ReliableUdpSession: outgoing packet registered in send window");

		if (!buildResult.has_value())
		{
			return;
		}

		const std::optional<common::net::ReliableUdpPacketView> packetView =
			common::net::ParseReliableUdpPacket(buildResult->data(), static_cast<int>(buildResult->size()));

		tests::Expect(result, packetView.has_value(), "ReliableUdpSession: outgoing reliable packet parsed");

		if (packetView.has_value())
		{
			tests::Expect(result, packetView->packetHeader.type == common::packet::PacketType::JoinRoomResponse,
				"ReliableUdpSession: outgoing reliable packet type");
			tests::Expect(result, packetView->reliableHeader.sequence == 1, "ReliableUdpSession: outgoing reliable sequence");
		}
	}
}

namespace tests::net
{
	tests::DebugTestResult RunReliableUdpSessionTests()
	{
		tests::DebugTestResult result{};

		reliableUdpSessionTest::RunBuildOutgoingHeaderWithoutAckTest(result);
		reliableUdpSessionTest::RunProcessReceivedDataHeaderUpdatesAckTest(result);
		reliableUdpSessionTest::RunBuildOutgoingHeaderWithAckTest(result);
		reliableUdpSessionTest::RunProcessReceivedDataHeaderRemovesAckedPendingPacketTest(result);
		reliableUdpSessionTest::RunProcessReceivedAckRemovesPendingPacketTest(result);
		reliableUdpSessionTest::RunExtractResendPacketsTest(result);
		reliableUdpSessionTest::RunResetTest(result);
		reliableUdpSessionTest::RunConfigureTest(result);
		reliableUdpSessionTest::RunBuildOutgoingPacketTest(result);

		return result;
	}
}