#pragma once

#include <WinSock2.h>

#include <atomic>
#include <functional>
#include <thread>

#include <Common/Net/Socket.h>

namespace client::net
{
	class UdpSocketTransport
	{
	public:
		using PacketReceivedCallback = std::function<void(const char* packetData, int packetSize)>;

	private:
		common::net::Socket socket_;
		sockaddr_in serverAddress_{};

		std::atomic<bool> isRunning_ = false;
		std::jthread recvThread_;

		PacketReceivedCallback packetReceivedCallback_;

	public:
		UdpSocketTransport() = default;
		~UdpSocketTransport() noexcept;

		UdpSocketTransport(const UdpSocketTransport&) = delete;
		UdpSocketTransport& operator=(const UdpSocketTransport&) = delete;

		UdpSocketTransport(UdpSocketTransport&&) = delete;
		UdpSocketTransport& operator=(UdpSocketTransport&&) = delete;

	public:
		[[nodiscard]] bool Start(const char* serverIp, unsigned short serverPort, PacketReceivedCallback packetReceivedCallback);
		void Stop() noexcept;

		[[nodiscard]] bool SendPacket(const void* packetData, int packetSize);

	private:
		[[nodiscard]] bool CreateSocket();
		[[nodiscard]] bool BindSocket();
		[[nodiscard]] bool ConfigureSocket();
		[[nodiscard]] bool SetServerAddress(const char* serverIp, unsigned short serverPort);

		void RecvLoop(std::stop_token stopToken);

	public:
		[[nodiscard]] bool IsRunning() const noexcept
		{
			return isRunning_.load();
		}
	};
}