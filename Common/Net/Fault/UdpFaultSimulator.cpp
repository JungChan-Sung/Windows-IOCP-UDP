#include "UdpFaultSimulator.h"

namespace common::net
{
	void UdpFaultSimulator::SetConfig(const Config& config)
	{
		// 이전 설정으로 예약된 지연 패킷이 새 설정에 남지 않도록 스케줄을 초기화
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