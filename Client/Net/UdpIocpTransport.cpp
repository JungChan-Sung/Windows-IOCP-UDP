#include "UdpIocpTransport.h"

#include <MSWSock.h>
#include <WS2tcpip.h>

#include <algorithm>
#include <utility>

namespace client::net
{
	UdpIocpTransport::~UdpIocpTransport() noexcept
	{
		Stop();
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

		packetReceivedCallback_ = nullptr;

		iocpHandle_.Close();

		serverAddress_ = {};
	}

	bool UdpIocpTransport::SendPacket(const void* packetData, int packetSize)
	{
		if (!socket_.IsValid() || packetData == nullptr || packetSize <= 0)
		{
			return false;
		}

		const int sentBytes = ::sendto(
			socket_.Get(),
			static_cast<const char*>(packetData),
			packetSize,
			0,
			reinterpret_cast<const sockaddr*>(&serverAddress_),
			sizeof(serverAddress_)
		);

		if (sentBytes == SOCKET_ERROR)
		{
			return false;
		}

		return sentBytes == packetSize;
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
		recvContextList_.clear();

		for (std::size_t index = 0; index < recvContextCount; ++index)
		{
			recvContextList_.emplace_back();
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
		workerThreadList_.clear();
		workerThreadList_.reserve(workerThreadCount);

		try
		{
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

			auto* recvContext = reinterpret_cast<common::net::UdpRecvContext*>(overlapped);

			if (!result || transferredBytes == 0)
			{
				if (isRunning_.load())
				{
					PostRecv(*recvContext);
				}

				continue;
			}

			if (IsFromServer(recvContext->remoteAddress) && packetReceivedCallback_)
			{
				packetReceivedCallback_(recvContext->buffer.data(), static_cast<int>(transferredBytes));
			}

			if (isRunning_.load())
			{
				PostRecv(*recvContext);
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

	bool UdpIocpTransport::IsFromServer(const sockaddr_in& remoteAddress) const noexcept
	{
		return remoteAddress.sin_addr.S_un.S_addr == serverAddress_.sin_addr.S_un.S_addr
			&& remoteAddress.sin_port == serverAddress_.sin_port;
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
}