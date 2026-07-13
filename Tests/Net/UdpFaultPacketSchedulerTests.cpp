#include "UdpFaultPacketSchedulerTests.h"

#include <WinSock2.h>

#include <chrono>
#include <cstdint>

#include <Common/Net/Fault/UdpFaultPacketScheduler.h>
#include <Common/Packet/PacketBuffer.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using Scheduler = common::net::UdpFaultPacketScheduler;
	using Decision = Scheduler::Decision;
	using PacketList = Scheduler::PacketList;
	using TimePoint = common::time::TimePoint;

	[[nodiscard]] sockaddr_in MakeRemoteAddress() noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_addr.S_un.S_addr = ::htonl(0x7F000001);
		remoteAddress.sin_port = ::htons(9000);
		return remoteAddress;
	}

	[[nodiscard]] bool IsSameAddress(const sockaddr_in& left, const sockaddr_in& right) noexcept
	{
		return left.sin_family == right.sin_family
			&& left.sin_addr.S_un.S_addr == right.sin_addr.S_un.S_addr
			&& left.sin_port == right.sin_port;
	}

	void RunDroppedPacketTest(tests::DebugTestResult& result)
	{
		Scheduler scheduler;

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'A', 'B', 'C' };

		Decision decision{};
		decision.shouldDrop = true;
		decision.shouldDuplicate = true;
		decision.delay = common::time::Milliseconds(100);

		const Scheduler::SubmitResult submitResult = scheduler.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(packetBuffer.data(), packetBuffer.size()),
			decision,
			TimePoint{}
		);

		tests::Expect(result, submitResult.dropped, "UdpFaultPacketScheduler: dropped packet reported");
		tests::Expect(result, submitResult.readyPacketList.empty(), "UdpFaultPacketScheduler: dropped packet not ready");
		tests::Expect(result, scheduler.GetPendingPacketCount() == 0, "UdpFaultPacketScheduler: dropped packet not queued");
	}

	void RunImmediatePacketTest(tests::DebugTestResult& result)
	{
		Scheduler scheduler;

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'A', 'B', 'C' };

		const Scheduler::SubmitResult submitResult = scheduler.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(packetBuffer.data(), packetBuffer.size()),
			Decision{},
			TimePoint{}
		);

		tests::Expect(result, !submitResult.dropped, "UdpFaultPacketScheduler: immediate packet not dropped");
		tests::Expect(result, submitResult.readyPacketList.size() == 1, "UdpFaultPacketScheduler: immediate packet ready");

		if (submitResult.readyPacketList.size() == 1)
		{
			tests::Expect(
				result,
				IsSameAddress(submitResult.readyPacketList[0].remoteAddress, remoteAddress),
				"UdpFaultPacketScheduler: immediate packet address"
			);

			tests::Expect(
				result,
				submitResult.readyPacketList[0].packetBuffer == packetBuffer,
				"UdpFaultPacketScheduler: immediate packet data"
			);
		}

		tests::Expect(result, scheduler.GetPendingPacketCount() == 0, "UdpFaultPacketScheduler: immediate packet not queued");
	}

	void RunImmediateDuplicatePacketTest(tests::DebugTestResult& result)
	{
		Scheduler scheduler;

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'D' };

		Decision decision{};
		decision.shouldDuplicate = true;

		const Scheduler::SubmitResult submitResult = scheduler.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(packetBuffer.data(), packetBuffer.size()),
			decision,
			TimePoint{}
		);

		tests::Expect(result, submitResult.readyPacketList.size() == 2, "UdpFaultPacketScheduler: immediate duplicate count");

		if (submitResult.readyPacketList.size() == 2)
		{
			tests::Expect(
				result,
				submitResult.readyPacketList[0].packetBuffer == packetBuffer,
				"UdpFaultPacketScheduler: first duplicate data"
			);

			tests::Expect(
				result,
				submitResult.readyPacketList[1].packetBuffer == packetBuffer,
				"UdpFaultPacketScheduler: second duplicate data"
			);
		}
	}

	void RunDelayedPacketTest(tests::DebugTestResult& result)
	{
		Scheduler scheduler;

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'E' };
		const TimePoint currentTime{};

		Decision decision{};
		decision.delay = common::time::Milliseconds(100);

		const Scheduler::SubmitResult submitResult = scheduler.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(packetBuffer.data(), packetBuffer.size()),
			decision,
			currentTime
		);

		tests::Expect(result, submitResult.readyPacketList.empty(), "UdpFaultPacketScheduler: delayed packet not immediately ready");
		tests::Expect(result, scheduler.GetPendingPacketCount() == 1, "UdpFaultPacketScheduler: delayed packet queued");

		const PacketList earlyPacketList = scheduler.ExtractReadyPackets(
			currentTime + common::time::Milliseconds(99)
		);

		tests::Expect(result, earlyPacketList.empty(), "UdpFaultPacketScheduler: delayed packet blocked before release");
		tests::Expect(result, scheduler.GetPendingPacketCount() == 1, "UdpFaultPacketScheduler: early extraction preserves packet");

		const PacketList readyPacketList = scheduler.ExtractReadyPackets(
			currentTime + common::time::Milliseconds(100)
		);

		tests::Expect(result, readyPacketList.size() == 1, "UdpFaultPacketScheduler: delayed packet released");
		tests::Expect(result, scheduler.GetPendingPacketCount() == 0, "UdpFaultPacketScheduler: released packet removed");

		if (readyPacketList.size() == 1)
		{
			tests::Expect(
				result,
				readyPacketList[0].packetBuffer == packetBuffer,
				"UdpFaultPacketScheduler: delayed packet data"
			);
		}
	}

	void RunReleaseOrderTest(tests::DebugTestResult& result)
	{
		Scheduler scheduler;

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer firstPacketBuffer{ 'A' };
		const common::packet::PacketBuffer secondPacketBuffer{ 'B' };
		const TimePoint currentTime{};

		Decision firstDecision{};
		firstDecision.shouldReorder = true;
		firstDecision.delay = common::time::Milliseconds(100);

		Decision secondDecision{};
		secondDecision.delay = common::time::Milliseconds(50);

		static_cast<void>(scheduler.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(firstPacketBuffer.data(), firstPacketBuffer.size()),
			firstDecision,
			currentTime
		));

		static_cast<void>(scheduler.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(secondPacketBuffer.data(), secondPacketBuffer.size()),
			secondDecision,
			currentTime
		));

		const PacketList readyPacketList = scheduler.ExtractReadyPackets(
			currentTime + common::time::Milliseconds(100)
		);

		tests::Expect(result, readyPacketList.size() == 2, "UdpFaultPacketScheduler: release order packet count");

		if (readyPacketList.size() == 2)
		{
			tests::Expect(
				result,
				readyPacketList[0].packetBuffer == secondPacketBuffer,
				"UdpFaultPacketScheduler: later packet released first"
			);

			tests::Expect(
				result,
				readyPacketList[1].packetBuffer == firstPacketBuffer,
				"UdpFaultPacketScheduler: reordered packet released last"
			);
		}
	}

	void RunResetClearsPendingPacketsTest(tests::DebugTestResult& result)
	{
		Scheduler scheduler;

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'A' };

		Decision decision{};
		decision.delay = std::chrono::seconds(10);

		static_cast<void>(scheduler.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(packetBuffer.data(), packetBuffer.size()),
			decision,
			TimePoint{}
		));

		scheduler.Reset();

		tests::Expect(result, scheduler.GetPendingPacketCount() == 0, "UdpFaultPacketScheduler: reset clears pending packets");
		tests::Expect(
			result,
			scheduler.ExtractReadyPackets(TimePoint{} + std::chrono::seconds(20)).empty(),
			"UdpFaultPacketScheduler: reset prevents later release"
		);
	}
}

namespace tests::net
{
	tests::DebugTestResult RunUdpFaultPacketSchedulerTests()
	{
		tests::DebugTestResult result{};

		RunDroppedPacketTest(result);
		RunImmediatePacketTest(result);
		RunImmediateDuplicatePacketTest(result);
		RunDelayedPacketTest(result);
		RunReleaseOrderTest(result);
		RunResetClearsPendingPacketsTest(result);

		return result;
	}
}