#pragma once

#include <WinSock2.h>

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional>
#include <stop_token>
#include <thread>
#include <vector>

#include <Common/Net/IocpHandle.h>
#include <Common/Net/Socket.h>
#include <Common/Net/UdpContext.h>

namespace server::net
{
	class UdpIocpTransport
	{
	public:
		using PacketReceivedHandler = std::function<void(const sockaddr_in&, const char*, int)>;

	private:
		using WorkerThreadList = std::vector<std::jthread>;
		using RecvContextList = std::deque<common::net::UdpRecvContext>;

	private:
		static inline constexpr std::size_t defaultRecvContextCount = 32;

	private:
		common::net::Socket socket_;
		common::net::IocpHandle iocpHandle_;
		std::atomic<bool> isRunning_ = false;

		WorkerThreadList workerThreadList_;
		RecvContextList recvContextList_;

		PacketReceivedHandler packetReceivedHandler_;

		unsigned short port_ = 0;
		std::size_t workerThreadCount_ = 0;

	public:
		UdpIocpTransport() = default;
		~UdpIocpTransport() noexcept;

		UdpIocpTransport(const UdpIocpTransport&) = delete;
		UdpIocpTransport& operator=(const UdpIocpTransport&) = delete;

		UdpIocpTransport(UdpIocpTransport&&) = delete;
		UdpIocpTransport& operator=(UdpIocpTransport&&) = delete;

	public:
		[[nodiscard]] bool Start(
			unsigned short port,
			std::size_t workerThreadCount,
			std::size_t recvContextCount,
			PacketReceivedHandler packetReceivedHandler
		);
		void Stop() noexcept;

	private:
		[[nodiscard]] bool CreateSocket();
		[[nodiscard]] bool BindSocket(unsigned short port);
		[[nodiscard]] bool CreateIocp();
		[[nodiscard]] bool CreateRecvContexts(std::size_t recvContextCount);
		[[nodiscard]] bool StartWorkerThreads(std::size_t workerThreadCount);

		[[nodiscard]] bool PostRecv(common::net::UdpRecvContext& recvContext);
		void WorkerLoop(std::stop_token stopToken);
		void StopWorkerThreads() noexcept;

	public:
		[[nodiscard]] common::net::Socket& GetSocket() noexcept
		{
			return socket_;
		}

		[[nodiscard]] const common::net::Socket& GetSocket() const noexcept
		{
			return socket_;
		}

		[[nodiscard]] unsigned short GetPort() const noexcept
		{
			return port_;
		}

		[[nodiscard]] std::size_t GetWorkerThreadCount() const noexcept
		{
			return workerThreadCount_;
		}

		[[nodiscard]] bool IsRunning() const noexcept
		{
			return isRunning_.load();
		}
	};
}

