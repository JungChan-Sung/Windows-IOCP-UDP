#include "UdpSocketTransport.h"

#include <MSWSock.h>
#include <WS2tcpip.h>

#include <chrono>
#include <thread>
#include <utility>

#include <Common/Net/UdpContext.h>

namespace client::net
{
	UdpSocketTransport::~UdpSocketTransport() noexcept
	{
		Stop();
	}

	bool UdpSocketTransport::Start(const char* serverIp, unsigned short serverPort, PacketReceivedCallback packetReceivedCallback)
	{
		if (isRunning_.load())
		{
			return false;
		}

		if (!packetReceivedCallback)
		{
			return false;
		}

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

		packetReceivedCallback_ = std::move(packetReceivedCallback);

		isRunning_.store(true);

		recvThread_ = std::jthread(
			[this](std::stop_token stopToken)
			{
				RecvLoop(stopToken);
			}
		);

		return true;
	}

	void UdpSocketTransport::Stop() noexcept
	{
		if (!isRunning_.exchange(false))
		{
			return;
		}

		if (recvThread_.joinable())
		{
			recvThread_.request_stop();
		}

		socket_.Close();

		if (recvThread_.joinable())
		{
			recvThread_.join();
		}

		recvThread_ = std::jthread();
		packetReceivedCallback_ = {};
		serverAddress_ = {};
	}

	bool UdpSocketTransport::SendPacket(const void* packetData, int packetSize)
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

	bool UdpSocketTransport::CreateSocket()
	{
		SOCKET handle = ::WSASocketW(
			AF_INET,
			SOCK_DGRAM,
			IPPROTO_UDP,
			nullptr,
			0,
			0
		);

		if (handle == INVALID_SOCKET)
		{
			return false;
		}

		socket_.Reset(handle);
		return true;
	}

	bool UdpSocketTransport::BindSocket()
	{
		sockaddr_in localAddress{};
		localAddress.sin_family = AF_INET;
		localAddress.sin_addr.s_addr = ::htonl(INADDR_ANY);
		localAddress.sin_port = ::htons(0);

		const int result = ::bind(socket_.Get(), reinterpret_cast<const sockaddr*>(&localAddress), sizeof(localAddress));

		return result != SOCKET_ERROR;
	}

	bool UdpSocketTransport::ConfigureSocket()
	{
		u_long nonBlocking = 1;

		const int nonBlockingResult = ::ioctlsocket(
			socket_.Get(),
			FIONBIO,
			&nonBlocking
		);

		if (nonBlockingResult == SOCKET_ERROR)
		{
			return false;
		}

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

	bool UdpSocketTransport::SetServerAddress(const char* serverIp, unsigned short serverPort)
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

	void UdpSocketTransport::RecvLoop(std::stop_token stopToken)
	{
		using namespace std::chrono_literals;

		common::net::UdpBuffer receiveBuffer{};

		while (!stopToken.stop_requested())
		{
			sockaddr_in remoteAddress{};
			int remoteAddressLength = static_cast<int>(sizeof(remoteAddress));

			const int receiveBytes = ::recvfrom(
				socket_.Get(),
				receiveBuffer.data(),
				static_cast<int>(receiveBuffer.size()),
				0,
				reinterpret_cast<sockaddr*>(&remoteAddress),
				&remoteAddressLength
			);

			if (receiveBytes == SOCKET_ERROR)
			{
				if (!isRunning_.load())
				{
					break;
				}

				const int errorCode = ::WSAGetLastError();
				if (errorCode == WSAEWOULDBLOCK)
				{
					std::this_thread::sleep_for(1ms);
					continue;
				}

				if (errorCode == WSAENOTSOCK || errorCode == WSAESHUTDOWN || errorCode == WSAEINTR)
				{
					break;
				}

				continue;
			}

			if (receiveBytes <= 0)
			{
				std::this_thread::sleep_for(1ms);
				continue;
			}

			if (remoteAddress.sin_addr.S_un.S_addr != serverAddress_.sin_addr.S_un.S_addr)
			{
				continue;
			}

			if (remoteAddress.sin_port != serverAddress_.sin_port)
			{
				continue;
			}

			if (packetReceivedCallback_)
			{
				packetReceivedCallback_(receiveBuffer.data(), receiveBytes);
			}
		}
	}
}