#pragma once

#include <WinSock2.h>

#include <atomic>
#include <cstddef>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string_view>
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
		enum class StartError
		{
			AlreadyRunning,
			InvalidCallback,
			CreateSocketFailed,
			BindSocketFailed,
			ConfigureSocketFailed,
			SetServerAddressFailed,
			CreateIocpFailed,
			StartWorkerThreadsFailed,
			CreateRecvContextsFailed,
		};

	public:
		using StartResult = std::expected<void, StartError>;

		using PacketReceivedCallback = std::function<void(const char* packetData, int packetSize)>;

	private:
		using WorkerThreadList = std::vector<std::jthread>;
		using RecvContextList = std::deque<common::net::UdpRecvContext>;
		using SendContextPointer = std::unique_ptr<common::net::UdpSendContext>;
		using PendingSendContextList = std::vector<SendContextPointer>;

	private:
		static inline constexpr std::size_t defaultRecvContextCount = 4;

	private:
		common::net::Socket socket_;
		common::net::IocpHandle iocpHandle_;
		sockaddr_in serverAddress_{};

		std::atomic<bool> isRunning_ = false;

		WorkerThreadList workerThreadList_;
		RecvContextList recvContextList_;

		mutable std::mutex pendingSendContextMutex_;
		PendingSendContextList pendingSendContextList_;

		PacketReceivedCallback packetReceivedCallback_;

	public:
		UdpIocpTransport() = default;
		~UdpIocpTransport() noexcept;

		UdpIocpTransport(const UdpIocpTransport&) = delete;
		UdpIocpTransport& operator=(const UdpIocpTransport&) = delete;

		UdpIocpTransport(UdpIocpTransport&&) = delete;
		UdpIocpTransport& operator=(UdpIocpTransport&&) = delete;

	public:
		[[nodiscard]] static std::string_view ToString(StartError startError) noexcept;

	public:
		[[nodiscard]] StartResult Start(
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
		[[nodiscard]] bool CreateRecvContexts(std::size_t recvContextCount);
		[[nodiscard]] bool StartWorkerThreads(std::size_t workerThreadCount);

		[[nodiscard]] bool PostRecv(common::net::UdpRecvContext& recvContext);
		void WorkerLoop(std::stop_token stopToken) noexcept;
		void StopWorkerThreads() noexcept;

		void HandleRecvCompletion(common::net::UdpRecvContext& recvContext, DWORD transferredBytes, BOOL completionResult);
		void HandleSendCompletion(common::net::UdpSendContext* sendContext) noexcept;

		void CompleteSend(common::net::UdpSendContext* sendContext) noexcept;
		void CompleteSendLocked(common::net::UdpSendContext* sendContext) noexcept;
		void ClearPendingSendContexts() noexcept;

		[[nodiscard]] bool IsFromServer(const sockaddr_in& remoteAddress) const noexcept;

	private:
		[[nodiscard]] static std::size_t ResolveWorkerThreadCount(std::size_t workerThreadCount) noexcept;
		[[nodiscard]] static std::size_t ResolveRecvContextCount(std::size_t recvContextCount, std::size_t workerThreadCount) noexcept;

	public:
		[[nodiscard]] bool IsRunning() const noexcept
		{
			return isRunning_.load();
		}
	};
}
