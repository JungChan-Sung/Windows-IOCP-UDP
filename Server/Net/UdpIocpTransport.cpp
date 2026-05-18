#include "UdpIocpTransport.h"

#include <MSWSock.h>

#include <algorithm>
#include <utility>

namespace server::net
{
	UdpIocpTransport::~UdpIocpTransport() noexcept
	{
		Stop();
	}

	std::size_t UdpIocpTransport::ResolveRecvContextCount(std::size_t recvContextCount, std::size_t workerThreadCount) noexcept
	{
		if (recvContextCount != 0)
		{
			return recvContextCount;
		}

		return std::max(defaultRecvContextCount, workerThreadCount * 2);
	}

	bool UdpIocpTransport::Start(unsigned short port, std::size_t workerThreadCount, std::size_t recvContextCount, PacketReceivedHandler packetReceivedHandler)
	{
		if (isRunning_.load())
		{
			return false;
		}

		if (!packetReceivedHandler)
		{
			return false;
		}

		port_ = port;
		workerThreadCount_ = workerThreadCount;
		packetReceivedHandler_ = std::move(packetReceivedHandler);

		if (!CreateSocket())
		{
			Stop();
			return false;
		}

		if (!BindSocket(port))
		{
			Stop();
			return false;
		}

		if (!ConfigureSocket())
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

		const std::size_t resolvedRecvContextCount = ResolveRecvContextCount(recvContextCount, workerThreadCount_);

		if (!StartWorkerThreads(workerThreadCount_))
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

		packetReceivedHandler_ = nullptr;

		iocpHandle_.Close();

		port_ = 0;
		workerThreadCount_ = 0;
	}

	bool UdpIocpTransport::SendPacket(const sockaddr_in& remoteAddress, const void* packetData, int packetSize) const
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
			reinterpret_cast<const sockaddr*>(&remoteAddress),
			sizeof(remoteAddress)
		);

		if (sentBytes == SOCKET_ERROR)
		{
			return false;
		}

		return sentBytes == packetSize;
	}

	bool UdpIocpTransport::CreateSocket()
	{
		SOCKET socketHandle = ::WSASocketW(
			AF_INET,
			SOCK_DGRAM,
			IPPROTO_UDP,
			nullptr,
			0,
			WSA_FLAG_OVERLAPPED
		);

		if (socketHandle == INVALID_SOCKET)
		{
			return false;
		}

		socket_.Reset(socketHandle);
		return true;
	}

	bool UdpIocpTransport::BindSocket(unsigned short port)
	{
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = ::htonl(INADDR_ANY);
		address.sin_port = ::htons(port);

		const int result = ::bind(socket_.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address));
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

	bool UdpIocpTransport::CreateIocp()
	{
		HANDLE iocpHandle = ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket_.Get()), nullptr, 0, 0);
		if (iocpHandle == nullptr)
		{
			return false;
		}

		iocpHandle_.Reset(iocpHandle);
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

	void UdpIocpTransport::WorkerLoop(std::stop_token stopToken)
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

			if (packetReceivedHandler_)
			{
				packetReceivedHandler_(
					recvContext->remoteAddress,
					recvContext->buffer.data(),
					static_cast<int>(transferredBytes)
				);
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
}