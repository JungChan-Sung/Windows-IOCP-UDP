#pragma once

#include <WinSock2.h>

#include <atomic>
#include <expected>
#include <functional>
#include <string_view>
#include <thread>

#include <Common/Net/Socket.h>

namespace client::net
{
	class UdpSocketTransport
	{
	public:
		enum class StartError
		{
			AlreadyRunning,
			InvalidCallback,
			CreateSocketFailed,
			BindSocketFailed,
			ConfigureSocketFailed,
			SetServerAddressFailed,
			StartRecvThreadFailed,
		};

	public:
		using StartResult = std::expected<void, StartError>;

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
		[[nodiscard]] static std::string_view ToString(StartError startError) noexcept;

	public:
		[[nodiscard]] StartResult Start(const char* serverIp, unsigned short serverPort, PacketReceivedCallback packetReceivedCallback);
		void Stop() noexcept;

		[[nodiscard]] bool SendPacket(const void* packetData, int packetSize);

	private:
		[[nodiscard]] bool CreateSocket();
		[[nodiscard]] bool BindSocket();
		[[nodiscard]] bool ConfigureSocket();
		[[nodiscard]] bool SetServerAddress(const char* serverIp, unsigned short serverPort);
		[[nodiscard]] bool StartRecvThread();

		void RecvLoop(std::stop_token stopToken);

	public:
		[[nodiscard]] bool IsRunning() const noexcept
		{
			return isRunning_.load();
		}
	};
}