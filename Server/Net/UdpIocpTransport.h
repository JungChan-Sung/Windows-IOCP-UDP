#pragma once

#include <WinSock2.h>

#include <atomic>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
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
		using SendContextPointer = std::unique_ptr<common::net::UdpSendContext>;
		using PendingSendContextList = std::vector<SendContextPointer>;

	private:
		static inline constexpr std::size_t defaultRecvContextCount = 32;

	private:
		common::net::Socket socket_;
		common::net::IocpHandle iocpHandle_;
		std::atomic<bool> isRunning_ = false;

		WorkerThreadList workerThreadList_;
		RecvContextList recvContextList_;

		mutable std::mutex pendingSendContextMutex_;
		PendingSendContextList pendingSendContextList_;

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

	private:
		[[nodiscard]] static std::size_t ResolveRecvContextCount(std::size_t recvContextCount, std::size_t workerThreadCount) noexcept;

	public:
		[[nodiscard]] bool Start(
			unsigned short port,
			std::size_t workerThreadCount,
			std::size_t recvContextCount,
			PacketReceivedHandler packetReceivedHandler
		);
		void Stop() noexcept;

		[[nodiscard]] bool SendPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize);

	private:
		[[nodiscard]] bool CreateSocket();
		[[nodiscard]] bool BindSocket(unsigned short port);
		[[nodiscard]] bool ConfigureSocket();
		[[nodiscard]] bool CreateIocp();
		[[nodiscard]] bool CreateRecvContexts(std::size_t recvContextCount);
		[[nodiscard]] bool StartWorkerThreads(std::size_t workerThreadCount);

		[[nodiscard]] bool PostRecv(common::net::UdpRecvContext& recvContext);
		void WorkerLoop(std::stop_token stopToken);
		void StopWorkerThreads() noexcept;

		void HandleRecvCompletion(common::net::UdpRecvContext& recvContext, DWORD transferredBytes, BOOL completionResult);
		void HandleSendCompletion(common::net::UdpSendContext* sendContext, DWORD transferredBytes, BOOL completionResult) noexcept;
		void CompleteSend(common::net::UdpSendContext* sendContext) noexcept;
		void CompleteSendLocked(common::net::UdpSendContext* sendContext) noexcept;
		void ClearPendingSendContexts() noexcept;

	public:
		[[nodiscard]] unsigned short GetPort() const noexcept
		{
			return port_;
		}

		[[nodiscard]] std::size_t GetWorkerThreadCount() const noexcept
		{
			return workerThreadCount_;
		}

		[[nodiscard]] std::size_t GetPendingSendContextCount() const noexcept;

		[[nodiscard]] bool IsRunning() const noexcept
		{
			return isRunning_.load();
		}
	};
}

