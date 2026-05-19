#include "UdpIocpTransport.h"

#include <MSWSock.h>
#include <WS2tcpip.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace client::net
{
	UdpIocpTransport::~UdpIocpTransport() noexcept
	{
		Stop();
	}

	std::size_t UdpIocpTransport::ResolveWorkerThreadCount(std::size_t workerThreadCount) noexcept
	{
		if (workerThreadCount != 0)
		{
			return workerThreadCount;
		}

		const unsigned int hardwareThreadCount = std::thread::hardware_concurrency();
		if (hardwareThreadCount == 0)
		{
			return 1;
		}

		return static_cast<std::size_t>(hardwareThreadCount);
	}

	std::size_t UdpIocpTransport::ResolveRecvContextCount(std::size_t recvContextCount, std::size_t workerThreadCount) noexcept
	{
		if (recvContextCount != 0)
		{
			return recvContextCount;
		}

		return std::max(defaultRecvContextCount, workerThreadCount * 2);
	}

	bool UdpIocpTransport::Start(
		const char* serverIp,
		unsigned short serverPort,
		std::size_t workerThreadCount,
		std::size_t recvContextCount,
		PacketReceivedCallback packetReceivedCallback
	)
	{
		if (isRunning_.load())
		{
			return false;
		}

		if (!packetReceivedCallback)
		{
			return false;
		}

		const std::size_t resolvedWorkerThreadCount = ResolveWorkerThreadCount(workerThreadCount);
		const std::size_t resolvedRecvContextCount = ResolveRecvContextCount(recvContextCount, resolvedWorkerThreadCount);

		packetReceivedCallback_ = std::move(packetReceivedCallback);

		if (!CreateSocket())
		{
			Stop();
			return false;
		}

		if (!BindSocket())
		{
			Stop();
			return false;
		}

		if (!ConfigureSocket())
		{
			Stop();
			return false;
		}

		if (!SetServerAddress(serverIp, serverPort))
		{
			Stop();
			return false;
		}

		if (!CreateIocp())
		{
			Stop();
			return false;
		}

		isRunning_.store(true);

		if (!StartWorkerThreads(resolvedWorkerThreadCount))
		{
			Stop();
			return false;
		}

		if (!CreateRecvContexts(resolvedRecvContextCount))
		{
			Stop();
			return false;
		}

		return true;
	}

	void UdpIocpTransport::Stop() noexcept
	{
		isRunning_.store(false);

		socket_.Close();

		StopWorkerThreads();

		recvContextList_.clear();
		ClearPendingSendContexts();

		packetReceivedCallback_ = nullptr;

		iocpHandle_.Close();

		serverAddress_ = {};
	}

	bool UdpIocpTransport::SendPacket(const void* packetData, int packetSize)
	{
		if (!isRunning_.load() || !socket_.IsValid() || packetData == nullptr || packetSize <= 0)
		{
			return false;
		}

		if (packetSize > static_cast<int>(common::net::udpBufferSize))
		{
			return false;
		}

		try
		{
			auto sendContext = std::make_unique<common::net::UdpSendContext>();
			sendContext->Prepare(
				serverAddress_,
				static_cast<const char*>(packetData),
				packetSize
			);

			common::net::UdpSendContext* rawSendContext = sendContext.get();

			std::scoped_lock lock(pendingSendContextMutex_);

			if (!isRunning_.load() || !socket_.IsValid())
			{
				return false;
			}

			pendingSendContextList_.push_back(std::move(sendContext));

			DWORD sentBytes = 0;

			const int result = ::WSASendTo(
				socket_.Get(),
				&rawSendContext->wsaBuffer,
				1,
				&sentBytes,
				0,
				reinterpret_cast<const sockaddr*>(&rawSendContext->remoteAddress),
				rawSendContext->remoteAddressLength,
				&rawSendContext->overlapped,
				nullptr
			);

			if (result == SOCKET_ERROR)
			{
				const int errorCode = ::WSAGetLastError();
				if (errorCode != WSA_IO_PENDING)
				{
					CompleteSendLocked(rawSendContext);
					return false;
				}
			}

			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool UdpIocpTransport::CreateSocket()
	{
		SOCKET handle = ::WSASocketW(
			AF_INET,
			SOCK_DGRAM,
			IPPROTO_UDP,
			nullptr,
			0,
			WSA_FLAG_OVERLAPPED
		);

		if (handle == INVALID_SOCKET)
		{
			return false;
		}

		socket_.Reset(handle);
		return true;
	}

	bool UdpIocpTransport::BindSocket()
	{
		sockaddr_in localAddress{};
		localAddress.sin_family = AF_INET;
		localAddress.sin_addr.s_addr = ::htonl(INADDR_ANY);
		localAddress.sin_port = ::htons(0);

		const int result = ::bind(socket_.Get(), reinterpret_cast<const sockaddr*>(&localAddress), sizeof(localAddress));
		return result != SOCKET_ERROR;
	}

	bool UdpIocpTransport::ConfigureSocket()
	{
		BOOL newBehavior = FALSE;
		DWORD bytesReturned = 0;

		const int connResetResult = ::WSAIoctl(
			socket_.Get(),
			SIO_UDP_CONNRESET,
			&newBehavior,
			sizeof(newBehavior),
			nullptr,
			0,
			&bytesReturned,
			nullptr,
			nullptr
		);

		return connResetResult != SOCKET_ERROR;
	}

	bool UdpIocpTransport::SetServerAddress(const char* serverIp, unsigned short serverPort)
	{
		if (serverIp == nullptr)
		{
			return false;
		}

		sockaddr_in serverAddress{};
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = ::htons(serverPort);

		const int result = ::InetPtonA(
			AF_INET,
			serverIp,
			&serverAddress.sin_addr
		);

		if (result != 1)
		{
			return false;
		}

		serverAddress_ = serverAddress;
		return true;
	}

	bool UdpIocpTransport::CreateIocp()
	{
		HANDLE handle = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket_.Get()), nullptr, 0, 0);
		if (handle == nullptr)
		{
			return false;
		}

		iocpHandle_.Reset(handle);
		return true;
	}

	bool UdpIocpTransport::CreateRecvContexts(std::size_t recvContextCount)
	{
		if (recvContextCount == 0)
		{
			return false;
		}

		recvContextList_.clear();

		try
		{
			for (std::size_t index = 0; index < recvContextCount; ++index)
			{
				recvContextList_.emplace_back();
			}
		}
		catch (...)
		{
			recvContextList_.clear();
			return false;
		}

		for (common::net::UdpRecvContext& recvContext : recvContextList_)
		{
			if (!PostRecv(recvContext))
			{
				return false;
			}
		}

		return true;
	}

	bool UdpIocpTransport::StartWorkerThreads(std::size_t workerThreadCount)
	{
		if (workerThreadCount == 0)
		{
			return false;
		}

		workerThreadList_.clear();

		try
		{
			workerThreadList_.reserve(workerThreadCount);

			for (std::size_t index = 0; index < workerThreadCount; ++index)
			{
				workerThreadList_.emplace_back(
					[this](std::stop_token stopToken)
					{
						WorkerLoop(stopToken);
					}
				);
			}
		}
		catch (...)
		{
			StopWorkerThreads();
			return false;
		}

		return true;
	}

	bool UdpIocpTransport::PostRecv(common::net::UdpRecvContext& recvContext)
	{
		recvContext.Reset();

		DWORD receivedBytes = 0;

		const int result = ::WSARecvFrom(
			socket_.Get(),
			&recvContext.wsaBuffer,
			1,
			&receivedBytes,
			&recvContext.flags,
			reinterpret_cast<sockaddr*>(&recvContext.remoteAddress),
			&recvContext.remoteAddressLength,
			&recvContext.overlapped,
			nullptr
		);

		if (result == SOCKET_ERROR)
		{
			const int errorCode = ::WSAGetLastError();
			if (errorCode != WSA_IO_PENDING)
			{
				return false;
			}
		}

		return true;
	}

	void UdpIocpTransport::WorkerLoop(std::stop_token stopToken) noexcept
	{
		while (!stopToken.stop_requested())
		{
			DWORD transferredBytes = 0;
			ULONG_PTR completionKey = 0;
			OVERLAPPED* overlapped = nullptr;

			const BOOL result = ::GetQueuedCompletionStatus(
				iocpHandle_.Get(),
				&transferredBytes,
				&completionKey,
				&overlapped,
				INFINITE
			);

			(void)completionKey;

			if (overlapped == nullptr)
			{
				if (!isRunning_.load() || stopToken.stop_requested())
				{
					break;
				}

				continue;
			}

			auto* udpContext = reinterpret_cast<common::net::UdpContext*>(overlapped);

			switch (udpContext->operationType)
			{
			case common::net::UdpOperationType::Recv:
				HandleRecvCompletion(
					*static_cast<common::net::UdpRecvContext*>(udpContext),
					transferredBytes,
					result
				);
				break;

			case common::net::UdpOperationType::Send:
				HandleSendCompletion(static_cast<common::net::UdpSendContext*>(udpContext));
				break;

			default:
				break;
			}
		}
	}

	void UdpIocpTransport::StopWorkerThreads() noexcept
	{
		for (std::jthread& workerThread : workerThreadList_)
		{
			workerThread.request_stop();
		}

		if (iocpHandle_.IsValid())
		{
			for (std::size_t index = 0; index < workerThreadList_.size(); ++index)
			{
				::PostQueuedCompletionStatus(iocpHandle_.Get(), 0, 0, nullptr);
			}
		}

		workerThreadList_.clear();
	}

	void UdpIocpTransport::HandleRecvCompletion(common::net::UdpRecvContext& recvContext, DWORD transferredBytes, BOOL completionResult)
	{
		if (!completionResult || transferredBytes == 0)
		{
			if (isRunning_.load())
			{
				PostRecv(recvContext);
			}

			return;
		}

		if (IsFromServer(recvContext.remoteAddress) && packetReceivedCallback_)
		{
			packetReceivedCallback_(recvContext.buffer.data(), static_cast<int>(transferredBytes));
		}

		if (isRunning_.load())
		{
			PostRecv(recvContext);
		}
	}

	void UdpIocpTransport::HandleSendCompletion(common::net::UdpSendContext* sendContext) noexcept
	{
		CompleteSend(sendContext);
	}

	void UdpIocpTransport::CompleteSend(common::net::UdpSendContext* sendContext) noexcept
	{
		std::scoped_lock lock(pendingSendContextMutex_);
		CompleteSendLocked(sendContext);
	}

	void UdpIocpTransport::CompleteSendLocked(common::net::UdpSendContext* sendContext) noexcept
	{
		if (sendContext == nullptr)
		{
			return;
		}

		const auto contextIterator = std::find_if(
			pendingSendContextList_.begin(),
			pendingSendContextList_.end(),
			[sendContext](const SendContextPointer& pendingSendContext)
			{
				return pendingSendContext.get() == sendContext;
			}
		);

		if (contextIterator != pendingSendContextList_.end())
		{
			pendingSendContextList_.erase(contextIterator);
		}
	}

	void UdpIocpTransport::ClearPendingSendContexts() noexcept
	{
		std::scoped_lock lock(pendingSendContextMutex_);
		pendingSendContextList_.clear();
	}

	bool UdpIocpTransport::IsFromServer(const sockaddr_in& remoteAddress) const noexcept
	{
		return remoteAddress.sin_addr.S_un.S_addr == serverAddress_.sin_addr.S_un.S_addr
			&& remoteAddress.sin_port == serverAddress_.sin_port;
	}
}