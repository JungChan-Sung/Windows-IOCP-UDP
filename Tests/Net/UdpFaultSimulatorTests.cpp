#include "UdpFaultSimulatorTests.h"

#include <WinSock2.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include <Common/Net/UdpFaultSimulator.h>
#include <Common/Packet/PacketBuffer.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using Simulator = common::net::UdpFaultSimulator;
	using Config = Simulator::Config;
	using PacketList = Simulator::PacketList;
	using SubmitResult = Simulator::SubmitResult;
	using TimePoint = common::time::TimePoint;

	struct SimulationOutcome
	{
	public:
		bool dropped = false;
		std::size_t readyPacketCount = 0;
	};

	[[nodiscard]] sockaddr_in MakeRemoteAddress() noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_addr.S_un.S_addr = ::htonl(0x7F000001);
		remoteAddress.sin_port = ::htons(9000);
		return remoteAddress;
	}

	[[nodiscard]] bool IsSameAddress(
		const sockaddr_in& left,
		const sockaddr_in& right
	) noexcept
	{
		return left.sin_family == right.sin_family
			&& left.sin_addr.S_un.S_addr == right.sin_addr.S_un.S_addr
			&& left.sin_port == right.sin_port;
	}

	[[nodiscard]] bool IsSameOutcome(
		const SimulationOutcome& left,
		const SimulationOutcome& right
	) noexcept
	{
		return left.dropped == right.dropped
			&& left.readyPacketCount == right.readyPacketCount;
	}

	void RunDisabledPassThroughTest(tests::DebugTestResult& result)
	{
		Simulator simulator;

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'A', 'B', 'C' };

		const SubmitResult submitResult = simulator.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(
				packetBuffer.data(),
				packetBuffer.size()
			),
			TimePoint{}
		);

		tests::Expect(
			result,
			!simulator.IsEnabled(),
			"UdpFaultSimulator: disabled by default"
		);
		tests::Expect(
			result,
			!submitResult.dropped,
			"UdpFaultSimulator: disabled packet not dropped"
		);
		tests::Expect(
			result,
			submitResult.readyPacketList.size() == 1,
			"UdpFaultSimulator: disabled packet immediately ready"
		);
		tests::Expect(
			result,
			simulator.GetPendingPacketCount() == 0,
			"UdpFaultSimulator: disabled packet not queued"
		);

		if (submitResult.readyPacketList.size() == 1)
		{
			tests::Expect(
				result,
				IsSameAddress(
					submitResult.readyPacketList[0].remoteAddress,
					remoteAddress
				),
				"UdpFaultSimulator: disabled packet address preserved"
			);
			tests::Expect(
				result,
				submitResult.readyPacketList[0].packetBuffer == packetBuffer,
				"UdpFaultSimulator: disabled packet data preserved"
			);
		}
	}

	void RunGuaranteedDropTest(tests::DebugTestResult& result)
	{
		Config config{};
		config.enabled = true;
		config.dropRate = 1.0F;

		Simulator simulator;
		simulator.SetConfig(config);

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'D' };

		const SubmitResult submitResult = simulator.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(
				packetBuffer.data(),
				packetBuffer.size()
			),
			TimePoint{}
		);

		tests::Expect(
			result,
			simulator.IsEnabled(),
			"UdpFaultSimulator: enabled after config"
		);
		tests::Expect(
			result,
			submitResult.dropped,
			"UdpFaultSimulator: guaranteed packet drop"
		);
		tests::Expect(
			result,
			submitResult.readyPacketList.empty(),
			"UdpFaultSimulator: dropped packet not ready"
		);
		tests::Expect(
			result,
			simulator.GetPendingPacketCount() == 0,
			"UdpFaultSimulator: dropped packet not queued"
		);
	}

	void RunImmediateDuplicateTest(tests::DebugTestResult& result)
	{
		Config config{};
		config.enabled = true;
		config.duplicateRate = 1.0F;

		Simulator simulator;
		simulator.SetConfig(config);

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'E', 'F' };

		const SubmitResult submitResult = simulator.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(
				packetBuffer.data(),
				packetBuffer.size()
			),
			TimePoint{}
		);

		tests::Expect(
			result,
			!submitResult.dropped,
			"UdpFaultSimulator: duplicated packet not dropped"
		);
		tests::Expect(
			result,
			submitResult.readyPacketList.size() == 2,
			"UdpFaultSimulator: duplicated packet count"
		);
		tests::Expect(
			result,
			simulator.GetPendingPacketCount() == 0,
			"UdpFaultSimulator: immediate duplicates not queued"
		);

		if (submitResult.readyPacketList.size() == 2)
		{
			tests::Expect(
				result,
				submitResult.readyPacketList[0].packetBuffer == packetBuffer,
				"UdpFaultSimulator: first duplicate data"
			);
			tests::Expect(
				result,
				submitResult.readyPacketList[1].packetBuffer == packetBuffer,
				"UdpFaultSimulator: second duplicate data"
			);
		}
	}

	void RunDelayedReleaseTest(tests::DebugTestResult& result)
	{
		Config config{};
		config.enabled = true;
		config.minDelay = std::chrono::milliseconds(100);
		config.maxDelay = std::chrono::milliseconds(100);

		Simulator simulator;
		simulator.SetConfig(config);

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'G' };
		const TimePoint currentTime{};

		const SubmitResult submitResult = simulator.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(
				packetBuffer.data(),
				packetBuffer.size()
			),
			currentTime
		);

		tests::Expect(
			result,
			submitResult.readyPacketList.empty(),
			"UdpFaultSimulator: delayed packet not immediately ready"
		);
		tests::Expect(
			result,
			simulator.GetPendingPacketCount() == 1,
			"UdpFaultSimulator: delayed packet queued"
		);

		const PacketList earlyPacketList = simulator.ExtractReadyPackets(
			currentTime + std::chrono::milliseconds(99)
		);

		tests::Expect(
			result,
			earlyPacketList.empty(),
			"UdpFaultSimulator: delayed packet blocked before release"
		);
		tests::Expect(
			result,
			simulator.GetPendingPacketCount() == 1,
			"UdpFaultSimulator: early extraction preserves packet"
		);

		const PacketList readyPacketList = simulator.ExtractReadyPackets(
			currentTime + std::chrono::milliseconds(100)
		);

		tests::Expect(
			result,
			readyPacketList.size() == 1,
			"UdpFaultSimulator: delayed packet released"
		);
		tests::Expect(
			result,
			simulator.GetPendingPacketCount() == 0,
			"UdpFaultSimulator: released packet removed"
		);

		if (readyPacketList.size() == 1)
		{
			tests::Expect(
				result,
				readyPacketList[0].packetBuffer == packetBuffer,
				"UdpFaultSimulator: delayed packet data"
			);
		}
	}

	void RunSetConfigClearsPendingPacketsTest(
		tests::DebugTestResult& result
	)
	{
		Config enabledConfig{};
		enabledConfig.enabled = true;
		enabledConfig.minDelay = std::chrono::seconds(10);
		enabledConfig.maxDelay = std::chrono::seconds(10);

		Simulator simulator;
		simulator.SetConfig(enabledConfig);

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'H' };
		const TimePoint currentTime{};

		static_cast<void>(simulator.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(
				packetBuffer.data(),
				packetBuffer.size()
			),
			currentTime
		));

		tests::Expect(
			result,
			simulator.GetPendingPacketCount() == 1,
			"UdpFaultSimulator: packet pending before config change"
		);

		simulator.SetConfig(Config{});

		tests::Expect(
			result,
			!simulator.IsEnabled(),
			"UdpFaultSimulator: disabled by new config"
		);
		tests::Expect(
			result,
			simulator.GetPendingPacketCount() == 0,
			"UdpFaultSimulator: config change clears pending packets"
		);
		tests::Expect(
			result,
			simulator.ExtractReadyPackets(
				currentTime + std::chrono::seconds(20)
			).empty(),
			"UdpFaultSimulator: cleared packet not released later"
		);

		const SubmitResult submitResult = simulator.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(
				packetBuffer.data(),
				packetBuffer.size()
			),
			currentTime
		);

		tests::Expect(
			result,
			submitResult.readyPacketList.size() == 1,
			"UdpFaultSimulator: disabled config restores pass-through"
		);
	}

	void RunResetClearsPendingPacketsTest(
		tests::DebugTestResult& result
	)
	{
		Config config{};
		config.enabled = true;
		config.minDelay = std::chrono::seconds(10);
		config.maxDelay = std::chrono::seconds(10);

		Simulator simulator;
		simulator.SetConfig(config);

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'I' };
		const TimePoint currentTime{};

		static_cast<void>(simulator.Submit(
			remoteAddress,
			common::packet::ConstPacketSpan(
				packetBuffer.data(),
				packetBuffer.size()
			),
			currentTime
		));

		simulator.Reset();

		tests::Expect(
			result,
			simulator.IsEnabled(),
			"UdpFaultSimulator: reset preserves enabled state"
		);
		tests::Expect(
			result,
			simulator.GetPendingPacketCount() == 0,
			"UdpFaultSimulator: reset clears pending packets"
		);
		tests::Expect(
			result,
			simulator.ExtractReadyPackets(
				currentTime + std::chrono::seconds(20)
			).empty(),
			"UdpFaultSimulator: reset packet not released later"
		);
	}

	void RunResetReplaysSequenceTest(tests::DebugTestResult& result)
	{
		Config config{};
		config.enabled = true;
		config.dropRate = 0.25F;
		config.duplicateRate = 0.35F;
		config.randomSeed = 12345;

		Simulator simulator;
		simulator.SetConfig(config);

		const sockaddr_in remoteAddress = MakeRemoteAddress();
		const common::packet::PacketBuffer packetBuffer{ 'J' };
		std::array<SimulationOutcome, 32> originalOutcomeList{};

		for (SimulationOutcome& outcome : originalOutcomeList)
		{
			const SubmitResult submitResult = simulator.Submit(
				remoteAddress,
				common::packet::ConstPacketSpan(
					packetBuffer.data(),
					packetBuffer.size()
				),
				TimePoint{}
			);

			outcome.dropped = submitResult.dropped;
			outcome.readyPacketCount =
				submitResult.readyPacketList.size();
		}

		simulator.Reset();

		for (const SimulationOutcome& originalOutcome :
			originalOutcomeList)
		{
			const SubmitResult submitResult = simulator.Submit(
				remoteAddress,
				common::packet::ConstPacketSpan(
					packetBuffer.data(),
					packetBuffer.size()
				),
				TimePoint{}
			);

			const SimulationOutcome replayedOutcome{
				submitResult.dropped,
				submitResult.readyPacketList.size()
			};

			tests::Expect(
				result,
				IsSameOutcome(originalOutcome, replayedOutcome),
				"UdpFaultSimulator: reset replays decision sequence"
			);
		}
	}
}

namespace tests::net
{
	tests::DebugTestResult RunUdpFaultSimulatorTests()
	{
		tests::DebugTestResult result{};

		RunDisabledPassThroughTest(result);
		RunGuaranteedDropTest(result);
		RunImmediateDuplicateTest(result);
		RunDelayedReleaseTest(result);
		RunSetConfigClearsPendingPacketsTest(result);
		RunResetClearsPendingPacketsTest(result);
		RunResetReplaysSequenceTest(result);

		return result;
	}
}