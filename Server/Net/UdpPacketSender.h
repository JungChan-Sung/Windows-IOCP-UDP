#pragma once

#include <WinSock2.h>

#include <span>

#include <Common/Net/EndpointKey.h>
#include <Common/Net/Fault/UdpFaultSimulator.h>

namespace server::net
{
	class UdpIocpTransport;

	class UdpPacketSender
	{
	private:
		UdpIocpTransport* udpTransport_ = nullptr;
		common::net::UdpFaultSimulator faultSimulator_;

	public:
		UdpPacketSender() = default;
		~UdpPacketSender() noexcept = default;

		UdpPacketSender(const UdpPacketSender&) = delete;
		UdpPacketSender& operator=(const UdpPacketSender&) = delete;

		UdpPacketSender(UdpPacketSender&&) = delete;
		UdpPacketSender& operator=(UdpPacketSender&&) = delete;

	public:
		void AttachTransport(UdpIocpTransport& udpTransport) noexcept;
		void DetachTransport() noexcept;

		[[nodiscard]] bool SendPacket(const common::net::EndpointKey& endpointKey, const void* packetData, int packetSize);
		[[nodiscard]] bool SendPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize);

		[[nodiscard]] std::size_t BroadcastPacket(std::span<const common::net::EndpointKey> endpointKeyList, const void* packetData, int packetSize);
		[[nodiscard]] std::size_t BroadcastPacket(std::span<const sockaddr_in> remoteAddressList, const void* packetData, int packetSize);

		void SetFaultSimulationConfig(const common::net::UdpFaultSimulationConfig& config);
		void ResetFaultSimulation();

		[[nodiscard]] std::size_t FlushFaultSimulationPackets();

	private:
		[[nodiscard]] bool SendRawPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize) const;

	public:
		[[nodiscard]] std::size_t GetFaultSimulationPendingPacketCount() const noexcept
		{
			return faultSimulator_.GetPendingPacketCount();
		}
	};
}

