#include "UdpIocpTransport.h"

#include <MSWSock.h>
#include <WS2tcpip.h>

#include <thread>
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

		if (!CreateSocket())
		{
			return false;
		}

		if (!BindSocket())
		{
			socket_.Close();
			return false;
		}

		if (!ConfigureSocket())
		{
			socket_.Close();
			return false;
		}

		if (!SetServerAddress(serverIp, serverPort))
		{
			socket_.Close();
			return false;
		}

		if (!CreateIocp())
		{
			socket_.Close();
			return false;
		}

		if (!AssociateSocketWithIocp())
		{
			iocpHandle_.Close();
			socket_.Close();
			return false;
		}

		if (!CreateRecvContexts(resolvedRecvContextCount))
		{
			iocpHandle_.Close();
			socket_.Close();
			return false;
		}

		packetReceivedCallback_ = std::move(packetReceivedCallback);
		isRunning_.store(true);

		StartWorkerThreads(resolvedWorkerThreadCount);

		return true;
	}

	void UdpIocpTransport::Stop() noexcept
	{
		if (!isRunning_.exchange(false))
		{
			return;
		}

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

		socket_.Close();

		for (std::jthread& workerThread : workerThreadList_)
		{
			if (workerThread.joinable())
			{
				workerThread.join();
			}
		}

		workerThreadList_.clear();
		recvContextList_.clear();

		iocpHandle_.Close();
		socket_.Close();

		packetReceivedCallback_ = {};
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
		HANDLE handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
		if (handle == nullptr)
		{
			return false;
		}

		iocpHandle_.Reset(handle);
		return true;
	}

	bool UdpIocpTransport::AssociateSocketWithIocp()
	{
		if (!iocpHandle_.IsValid() || !socket_.IsValid())
		{
			return false;
		}

		HANDLE result = ::CreateIoCompletionPort(
			reinterpret_cast<HANDLE>(socket_.Get()),
			iocpHandle_.Get(),
			0,
			0
		);

		return result == iocpHandle_.Get();
	}

	bool UdpIocpTransport::CreateRecvContexts(std::size_t recvContextCount)
	{
		if (recvContextCount == 0)
		{
			return false;
		}

		try
		{
			recvContextList_.reserve(recvContextCount);

			for (std::size_t index = 0; index < recvContextCount; ++index)
			{
				recvContextList_.push_back(std::make_unique<common::net::UdpRecvContext>());
			}
		}
		catch (...)
		{
			recvContextList_.clear();
			return false;
		}

		return true;
	}

	void UdpIocpTransport::StartWorkerThreads(std::size_t workerThreadCount)
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

			if (stopToken.stop_requested() || !isRunning_.load())
			{
				return;
			}

			if (overlapped == nullptr)
			{
				return;
			}

			if (result == FALSE)
			{
				continue;
			}

			// Next step:
			// 1. Cast overlapped to common::net::UdpContext*
			// 2. Branch by UdpOperationType
			// 3. Handle recv completion
			// 4. Repost WSARecvFrom
		}
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

		const std::size_t resolvedRecvContextCount = workerThreadCount * 2;
		return (resolvedRecvContextCount != 0) ? resolvedRecvContextCount : 1;
	}
}