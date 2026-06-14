#pragma once

#include <WinSock2.h>

#include <atomic>
#include <chrono>
#include <cstddef>

#include <Common/Packet/PacketBuffer.h>
#include <Common/Time/TimeTypes.h>

#include "UdpFaultDecisionGenerator.h"
#include "UdpFaultPacketScheduler.h"
#include "UdpFaultSimulationConfig.h"

namespace common::net
{
	class UdpFaultSimulator
	{
	public:
		using Config = UdpFaultSimulationConfig;
		using Packet = UdpFaultPacketScheduler::Packet;
		using PacketList = UdpFaultPacketScheduler::PacketList;
		using SubmitResult = UdpFaultPacketScheduler::SubmitResult;

	private:
		std::atomic<bool> isEnabled_ = false;
		UdpFaultDecisionGenerator decisionGenerator_;
		UdpFaultPacketScheduler packetScheduler_;

	public:
		UdpFaultSimulator() = default;
		~UdpFaultSimulator() noexcept = default;

		UdpFaultSimulator(const UdpFaultSimulator&) = delete;
		UdpFaultSimulator& operator=(const UdpFaultSimulator&) = delete;

		UdpFaultSimulator(UdpFaultSimulator&&) = delete;
		UdpFaultSimulator& operator=(UdpFaultSimulator&&) = delete;

	public:
		void SetConfig(const Config& config);
		void Reset();

		[[nodiscard]] SubmitResult Submit(
			const sockaddr_in& remoteAddress,
			common::packet::ConstPacketSpan packetData,
			time::TimePoint currentTime
		);

		[[nodiscard]] PacketList ExtractReadyPackets(time::TimePoint currentTime);

	public:
		[[nodiscard]] bool IsEnabled() const noexcept
		{
			return isEnabled_.load();
		}

		[[nodiscard]] std::size_t GetPendingPacketCount() const noexcept
		{
			return packetScheduler_.GetPendingPacketCount();
		}
	};
}