#pragma once

#include <WinSock2.h>
#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include <Common/Net/IocpHandle.h>
#include <Common/Net/Socket.h>
#include <Common/Net/UdpContext.h>

namespace client::net
{
	class UdpIocpTransport
	{
	public:
		using PacketReceivedCallback = std::function<void(const char* packetData, int packetSize)>;

	private:
		using WorkerThreadList = std::vector<std::jthread>;
		using RecvContextList = std::vector<std::unique_ptr<common::net::UdpRecvContext>>;

	private:
		common::net::Socket socket_;
		common::net::IocpHandle iocpHandle_;
		sockaddr_in serverAddress_{};

		std::atomic<bool> isRunning_ = false;

		WorkerThreadList workerThreadList_;
		RecvContextList recvContextList_;

		PacketReceivedCallback packetReceivedCallback_;

	public:
		UdpIocpTransport() = default;
		~UdpIocpTransport() noexcept;

		UdpIocpTransport(const UdpIocpTransport&) = delete;
		UdpIocpTransport& operator=(const UdpIocpTransport&) = delete;

		UdpIocpTransport(UdpIocpTransport&&) = delete;
		UdpIocpTransport& operator=(UdpIocpTransport&&) = delete;

	public:
		[[nodiscard]] bool Start(
			const char* serverIp,
			unsigned short serverPort,
			std::size_t workerThreadCount,
			std::size_t recvContextCount,
			PacketReceivedCallback packetReceivedCallback
		);
		void Stop() noexcept;

		[[nodiscard]] bool SendPacket(const void* packetData, int packetSize);

	private:
		[[nodiscard]] bool CreateSocket();
		[[nodiscard]] bool BindSocket();
		[[nodiscard]] bool ConfigureSocket();
		[[nodiscard]] bool SetServerAddress(const char* serverIp, unsigned short serverPort);
		[[nodiscard]] bool CreateIocp();
		[[nodiscard]] bool AssociateSocketWithIocp();
		[[nodiscard]] bool CreateRecvContexts(std::size_t recvContextCount);

		void StartWorkerThreads(std::size_t workerThreadCount);
		void WorkerLoop(std::stop_token stopToken) noexcept;

	private:
		[[nodiscard]] static std::size_t ResolveWorkerThreadCount(std::size_t workerThreadCount) noexcept;
		[[nodiscard]] static std::size_t ResolveRecvContextCount(
			std::size_t recvContextCount,
			std::size_t workerThreadCount
		) noexcept;

	public:
		[[nodiscard]] bool IsRunning() const noexcept
		{
			return isRunning_.load();
		}
	};
}
