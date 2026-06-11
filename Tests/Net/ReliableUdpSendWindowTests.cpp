#include "ReliableUdpSendWindowTests.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <Common/Net/ReliableUdpSendWindow.h>
#include <Common/Packet/PacketBuffer.h>

#include <Tests/DebugTestResult.h>

namespace
{
	[[nodiscard]] common::packet::PacketBuffer MakePacketBuffer(char value)
	{
		return common::packet::PacketBuffer{ value };
	}

	void RunRegisterSentPacketTests(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSendWindow sendWindow;
		const common::net::ReliableUdpSendWindow::TimePoint currentTime =
			common::net::ReliableUdpSendWindow::Clock::now();

		const std::optional<common::net::ReliableSequence> firstSequence =
			sendWindow.RegisterSentPacket(MakePacketBuffer('A'), currentTime);

		const std::optional<common::net::ReliableSequence> secondSequence =
			sendWindow.RegisterSentPacket(MakePacketBuffer('B'), currentTime);

		tests::Expect(result, firstSequence.has_value(), "ReliableUdpSendWindow: first packet registered");
		tests::Expect(result, secondSequence.has_value(), "ReliableUdpSendWindow: second packet registered");

		if (firstSequence.has_value() && secondSequence.has_value())
		{
			tests::Expect(result, *firstSequence == 1, "ReliableUdpSendWindow: first sequence is 1");
			tests::Expect(result, *secondSequence == 2, "ReliableUdpSendWindow: second sequence is 2");
		}

		tests::Expect(result, sendWindow.GetPendingPacketCount() == 2, "ReliableUdpSendWindow: pending count after register");
		tests::Expect(result, sendWindow.GetNextSequence() == 3, "ReliableUdpSendWindow: next sequence after register");
	}

	void RunRejectEmptyPacketTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSendWindow sendWindow;

		const std::optional<common::net::ReliableSequence> sequence =
			sendWindow.RegisterSentPacket({}, common::net::ReliableUdpSendWindow::Clock::now());

		tests::Expect(result, !sequence.has_value(), "ReliableUdpSendWindow: empty packet rejected");
		tests::Expect(result, sendWindow.GetPendingPacketCount() == 0, "ReliableUdpSendWindow: empty packet not pending");
	}

	void RunWindowFullTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSendWindow sendWindow;
		sendWindow.SetMaxPendingPacketCount(1);

		const common::net::ReliableUdpSendWindow::TimePoint currentTime =
			common::net::ReliableUdpSendWindow::Clock::now();

		const std::optional<common::net::ReliableSequence> firstSequence =
			sendWindow.RegisterSentPacket(MakePacketBuffer('A'), currentTime);

		const std::optional<common::net::ReliableSequence> secondSequence =
			sendWindow.RegisterSentPacket(MakePacketBuffer('B'), currentTime);

		tests::Expect(result, firstSequence.has_value(), "ReliableUdpSendWindow: first packet accepted before full");
		tests::Expect(result, !secondSequence.has_value(), "ReliableUdpSendWindow: second packet rejected when full");
		tests::Expect(result, sendWindow.GetPendingPacketCount() == 1, "ReliableUdpSendWindow: full window pending count");
	}

	void RunProcessAckTests(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSendWindow sendWindow;

		const common::net::ReliableUdpSendWindow::TimePoint currentTime =
			common::net::ReliableUdpSendWindow::Clock::now();

		sendWindow.RegisterSentPacket(MakePacketBuffer('A'), currentTime); // 1
		sendWindow.RegisterSentPacket(MakePacketBuffer('B'), currentTime); // 2
		sendWindow.RegisterSentPacket(MakePacketBuffer('C'), currentTime); // 3

		const bool directAckProcessed = sendWindow.ProcessAck(2, 0);

		tests::Expect(result, directAckProcessed, "ReliableUdpSendWindow: direct ack processed");
		tests::Expect(result, sendWindow.GetPendingPacketCount() == 2, "ReliableUdpSendWindow: ack removes one packet");

		const std::uint32_t ackBitfield = static_cast<std::uint32_t>(1) << 1;
		const bool bitfieldAckProcessed = sendWindow.ProcessAck(3, ackBitfield);

		tests::Expect(result, bitfieldAckProcessed, "ReliableUdpSendWindow: ack bitfield processed");
		tests::Expect(result, sendWindow.GetPendingPacketCount() == 0, "ReliableUdpSendWindow: ack bitfield removes remaining packets");
	}

	void RunZeroAckTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSendWindow sendWindow;

		const common::net::ReliableUdpSendWindow::TimePoint currentTime =
			common::net::ReliableUdpSendWindow::Clock::now();

		const std::optional<common::net::ReliableSequence> sequence =
			sendWindow.RegisterSentPacket(MakePacketBuffer('A'), currentTime);

		tests::Expect(result, sequence.has_value(), "ReliableUdpSendWindow: packet registered before zero ack");

		const bool ackProcessed = sendWindow.ProcessAck(0, 0xFFFFFFFF);

		tests::Expect(result, ackProcessed, "ReliableUdpSendWindow: zero ack accepted");
		tests::Expect(result, sendWindow.GetPendingPacketCount() == 1, "ReliableUdpSendWindow: zero ack keeps pending packet");
	}

	void RunFutureAckRejectedTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSendWindow sendWindow;

		const common::net::ReliableUdpSendWindow::TimePoint currentTime =
			common::net::ReliableUdpSendWindow::Clock::now();

		const std::optional<common::net::ReliableSequence> sequence =
			sendWindow.RegisterSentPacket(MakePacketBuffer('A'), currentTime);

		tests::Expect(result, sequence.has_value(), "ReliableUdpSendWindow: packet registered before future ack");

		if (!sequence.has_value())
		{
			return;
		}

		const common::net::ReliableSequence futureAckSequence = *sequence + 10;
		const bool ackProcessed = sendWindow.ProcessAck(futureAckSequence, 0xFFFFFFFF);

		tests::Expect(result, !ackProcessed, "ReliableUdpSendWindow: future ack rejected");
		tests::Expect(result, sendWindow.GetPendingPacketCount() == 1, "ReliableUdpSendWindow: future ack keeps pending packet");
	}

	void RunExtractResendPacketsTests(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSendWindow sendWindow;
		sendWindow.SetResendInterval(std::chrono::milliseconds(100));

		const common::net::ReliableUdpSendWindow::TimePoint startTime =
			common::net::ReliableUdpSendWindow::Clock::now();

		sendWindow.RegisterSentPacket(MakePacketBuffer('A'), startTime);
		sendWindow.RegisterSentPacket(MakePacketBuffer('B'), startTime);

		const common::net::ReliableUdpSendWindow::ResendPacketList earlyResendList =
			sendWindow.ExtractResendPackets(startTime + std::chrono::milliseconds(50));

		tests::Expect(result, earlyResendList.empty(), "ReliableUdpSendWindow: early resend empty");

		const common::net::ReliableUdpSendWindow::ResendPacketList resendList =
			sendWindow.ExtractResendPackets(startTime + std::chrono::milliseconds(100));

		tests::Expect(result, resendList.size() == 2, "ReliableUdpSendWindow: due resend count");

		if (resendList.size() == 2)
		{
			tests::Expect(result, resendList[0].sequence == 1, "ReliableUdpSendWindow: first resend sequence");
			tests::Expect(result, resendList[1].sequence == 2, "ReliableUdpSendWindow: second resend sequence");
			tests::Expect(result, resendList[0].resendCount == 1, "ReliableUdpSendWindow: first resend count");
			tests::Expect(result, resendList[1].resendCount == 1, "ReliableUdpSendWindow: second resend count");
		}

		const common::net::ReliableUdpSendWindow::ResendPacketList secondEarlyResendList =
			sendWindow.ExtractResendPackets(startTime + std::chrono::milliseconds(150));

		tests::Expect(result, secondEarlyResendList.empty(), "ReliableUdpSendWindow: second early resend empty");
	}

	void RunExtractResendResultGiveUpTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSendWindow sendWindow;
		sendWindow.SetMaxResendCount(1);
		sendWindow.SetResendInterval(std::chrono::milliseconds(100));

		const common::net::ReliableUdpSendWindow::TimePoint startTime =
			common::net::ReliableUdpSendWindow::Clock::now();

		const std::optional<common::net::ReliableSequence> sequence =
			sendWindow.RegisterSentPacket(MakePacketBuffer('A'), startTime);

		tests::Expect(result, sequence.has_value(), "ReliableUdpSendWindow: packet registered before give-up test");

		if (!sequence.has_value())
		{
			return;
		}

		const common::net::ReliableUdpSendWindow::ResendResult firstResult =
			sendWindow.ExtractResendResult(startTime + std::chrono::milliseconds(100));

		tests::Expect(result, firstResult.resendPacketList.size() == 1, "ReliableUdpSendWindow: first timeout resends packet");
		tests::Expect(result, firstResult.giveUpPacketList.empty(), "ReliableUdpSendWindow: first timeout does not give up packet");

		if (firstResult.resendPacketList.size() == 1)
		{
			tests::Expect(result, firstResult.resendPacketList[0].sequence == *sequence, "ReliableUdpSendWindow: resend sequence before give-up");
			tests::Expect(result, firstResult.resendPacketList[0].resendCount == 1, "ReliableUdpSendWindow: resend count before give-up");
		}

		tests::Expect(result, sendWindow.GetPendingPacketCount() == 1, "ReliableUdpSendWindow: resent packet remains pending");

		const common::net::ReliableUdpSendWindow::ResendResult secondResult =
			sendWindow.ExtractResendResult(startTime + std::chrono::milliseconds(200));

		tests::Expect(result, secondResult.resendPacketList.empty(), "ReliableUdpSendWindow: give-up timeout does not resend packet");
		tests::Expect(result, secondResult.giveUpPacketList.size() == 1, "ReliableUdpSendWindow: give-up timeout extracts packet");

		if (secondResult.giveUpPacketList.size() == 1)
		{
			tests::Expect(result, secondResult.giveUpPacketList[0].sequence == *sequence, "ReliableUdpSendWindow: give-up sequence");
			tests::Expect(result, secondResult.giveUpPacketList[0].resendCount == 1, "ReliableUdpSendWindow: give-up resend count");
		}

		tests::Expect(result, sendWindow.GetPendingPacketCount() == 0, "ReliableUdpSendWindow: give-up removes pending packet");
	}

	void RunExtractResendResultImmediateGiveUpTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSendWindow sendWindow;
		sendWindow.SetMaxResendCount(0);
		sendWindow.SetResendInterval(std::chrono::milliseconds(100));

		const common::net::ReliableUdpSendWindow::TimePoint startTime =
			common::net::ReliableUdpSendWindow::Clock::now();

		const std::optional<common::net::ReliableSequence> sequence =
			sendWindow.RegisterSentPacket(MakePacketBuffer('A'), startTime);

		tests::Expect(result, sequence.has_value(), "ReliableUdpSendWindow: packet registered before immediate give-up test");

		if (!sequence.has_value())
		{
			return;
		}

		const common::net::ReliableUdpSendWindow::ResendResult resultValue =
			sendWindow.ExtractResendResult(startTime + std::chrono::milliseconds(100));

		tests::Expect(result, resultValue.resendPacketList.empty(), "ReliableUdpSendWindow: max resend 0 does not resend packet");
		tests::Expect(result, resultValue.giveUpPacketList.size() == 1, "ReliableUdpSendWindow: max resend 0 gives up packet");

		if (resultValue.giveUpPacketList.size() == 1)
		{
			tests::Expect(result, resultValue.giveUpPacketList[0].sequence == *sequence, "ReliableUdpSendWindow: immediate give-up sequence");
			tests::Expect(result, resultValue.giveUpPacketList[0].resendCount == 0, "ReliableUdpSendWindow: immediate give-up resend count");
		}

		tests::Expect(result, sendWindow.GetPendingPacketCount() == 0, "ReliableUdpSendWindow: immediate give-up removes pending packet");
	}

	void RunResetTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpSendWindow sendWindow;

		sendWindow.RegisterSentPacket(
			MakePacketBuffer('A'),
			common::net::ReliableUdpSendWindow::Clock::now()
		);

		sendWindow.Reset();

		tests::Expect(result, sendWindow.GetPendingPacketCount() == 0, "ReliableUdpSendWindow: reset clears pending packets");
		tests::Expect(result, sendWindow.GetNextSequence() == 1, "ReliableUdpSendWindow: reset sequence");
	}
}

namespace tests::net
{
	tests::DebugTestResult RunReliableUdpSendWindowTests()
	{
		tests::DebugTestResult result{};

		RunRegisterSentPacketTests(result);
		RunRejectEmptyPacketTest(result);
		RunWindowFullTest(result);

		RunProcessAckTests(result);
		RunZeroAckTest(result);
		RunFutureAckRejectedTest(result);

		RunExtractResendPacketsTests(result);
		RunExtractResendResultGiveUpTest(result);
		RunExtractResendResultImmediateGiveUpTest(result);

		RunResetTest(result);

		return result;
	}
}