#include "ReliableUdpSendWindowTests.h"

#include <chrono>
#include <cstdint>
#include <vector>

#include <Common/Net/ReliableUdpSendWindow.h>

#include <Tests/DebugTestResult.h>

namespace
{
	[[nodiscard]] std::vector<char> MakePacketBuffer(char value)
	{
		return std::vector<char>{ value };
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

		sendWindow.ProcessAck(2, 0);

		tests::Expect(result, sendWindow.GetPendingPacketCount() == 2, "ReliableUdpSendWindow: ack removes one packet");

		const std::uint32_t ackBitfield = static_cast<std::uint32_t>(1) << 1;
		sendWindow.ProcessAck(3, ackBitfield);

		tests::Expect(result, sendWindow.GetPendingPacketCount() == 0, "ReliableUdpSendWindow: ack bitfield removes remaining packets");
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
		RunExtractResendPacketsTests(result);
		RunResetTest(result);

		return result;
	}
}