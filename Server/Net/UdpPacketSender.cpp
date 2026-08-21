#include "UdpPacketSender.h"

#include <Common/Net/Endpoint.h>

#include <Server/Net/UdpIocpTransport.h>

namespace server::net
{
	void UdpPacketSender::AttachTransport(UdpIocpTransport& udpTransport) noexcept
	{
		udpTransport_ = &udpTransport;
	}

	void UdpPacketSender::DetachTransport() noexcept
	{
		udpTransport_ = nullptr;
	}

	bool UdpPacketSender::SendPacket(const common::net::EndpointKey& endpointKey, const void* packetData, int packetSize)
	{
		return SendPacket(common::net::MakeSocketAddress(endpointKey), packetData, packetSize);
	}

	bool UdpPacketSender::SendPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize)
	{
		if (udpTransport_ == nullptr || packetData == nullptr || packetSize <= 0)
		{
			return false;
		}

		if (!faultSimulator_.IsEnabled())
		{
			return SendRawPacket(remoteAddress, packetData, packetSize);
		}

		const auto* packetBytes = static_cast<const char*>(packetData);

		common::net::UdpFaultSimulator::SubmitResult submitResult =
			faultSimulator_.Submit(
				remoteAddress,
				common::packet::ConstPacketSpan(
					packetBytes,
					static_cast<std::size_t>(packetSize)
				),
				common::time::Clock::now()
			);

		if (submitResult.dropped)
		{
			return true;
		}

		bool allSent = true;

		for (const common::net::UdpFaultSimulator::Packet& packet : submitResult.readyPacketList)
		{
			if (!SendRawPacket(
				packet.remoteAddress,
				packet.packetBuffer.data(),
				static_cast<int>(packet.packetBuffer.size())
			))
			{
				allSent = false;
			}
		}

		return allSent;
	}

	std::size_t UdpPacketSender::BroadcastPacket(std::span<const common::net::EndpointKey> endpointKeyList, const void* packetData, int packetSize)
	{
		std::size_t sentCount = 0;

		for (const common::net::EndpointKey& endpointKey : endpointKeyList)
		{
			if (SendPacket(endpointKey, packetData, packetSize))
			{
				++sentCount;
			}
		}

		return sentCount;
	}

	std::size_t UdpPacketSender::BroadcastPacket(std::span<const sockaddr_in> remoteAddressList, const void* packetData, int packetSize)
	{
		std::size_t sentCount = 0;

		for (const sockaddr_in& remoteAddress : remoteAddressList)
		{
			if (SendPacket(remoteAddress, packetData, packetSize))
			{
				++sentCount;
			}
		}

		return sentCount;
	}

	void UdpPacketSender::SetFaultSimulationConfig(const common::net::UdpFaultSimulationConfig& config)
	{
		faultSimulator_.SetConfig(config);
	}

	void UdpPacketSender::ResetFaultSimulation()
	{
		faultSimulator_.Reset();
	}

	std::size_t UdpPacketSender::FlushFaultSimulationPackets()
	{
		common::net::UdpFaultSimulator::PacketList readyPacketList = faultSimulator_.ExtractReadyPackets(common::time::Clock::now());
		std::size_t sentCount = 0;

		for (const common::net::UdpFaultSimulator::Packet& packet : readyPacketList)
		{
			if (SendRawPacket(packet.remoteAddress, packet.packetBuffer.data(), static_cast<int>(packet.packetBuffer.size())))
			{
				++sentCount;
			}
		}

		return sentCount;
	}

	bool UdpPacketSender::SendRawPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize) const
	{
		if (udpTransport_ == nullptr || packetData == nullptr || packetSize <= 0)
		{
			return false;
		}

		return udpTransport_->SendPacket(remoteAddress, packetData, packetSize);
	}
}