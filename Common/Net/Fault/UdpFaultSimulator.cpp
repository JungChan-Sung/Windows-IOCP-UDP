#include "UdpFaultSimulator.h"

namespace common::net
{
	void UdpFaultSimulator::SetConfig(const Config& config)
	{
		packetScheduler_.Reset();
		decisionGenerator_.SetConfig(config);
		isEnabled_.store(config.enabled);
	}

	void UdpFaultSimulator::Reset()
	{
		decisionGenerator_.Reset();
		packetScheduler_.Reset();
	}

	UdpFaultSimulator::SubmitResult UdpFaultSimulator::Submit(const sockaddr_in& remoteAddress, packet::ConstPacketSpan packetData, time::TimePoint currentTime)
	{
		UdpFaultDecisionGenerator::Decision decision{};

		if (isEnabled_.load())
		{
			decision = decisionGenerator_.Generate();
		}

		return packetScheduler_.Submit(
			remoteAddress,
			packetData,
			decision,
			currentTime
		);
	}
	 
	UdpFaultSimulator::PacketList UdpFaultSimulator::ExtractReadyPackets(time::TimePoint currentTime)
	{
		return packetScheduler_.ExtractReadyPackets(currentTime);
	}
}