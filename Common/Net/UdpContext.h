#pragma once

#include <WinSock2.h>

#include <array>
#include <cstddef>

#include "UdpTypes.h"

namespace common::net
{
	inline constexpr std::size_t udpBufferSize = 1200;

	using UdpBuffer = std::array<char, common::net::udpBufferSize>;

	struct UdpContext
	{
	public:
		OVERLAPPED overlapped{};
		WSABUF wsaBuffer{};
		UdpOperationType operationType = UdpOperationType::Recv;

	public:
		UdpContext() = default;
		~UdpContext() = default;

		UdpContext(const UdpContext& other) = delete;
		UdpContext& operator=(const UdpContext& other) = delete;

		UdpContext(UdpContext&& other) = delete;
		UdpContext& operator=(UdpContext&& other) = delete;
	};

	struct UdpRecvContext : public UdpContext
	{
	public:
		UdpBuffer buffer{};
		sockaddr_in remoteAddress{};
		int remoteAddressLength = static_cast<int>(sizeof(remoteAddress));
		DWORD flags = 0;

	public:
		UdpRecvContext();

	public:
		void Reset() noexcept;
	};

	struct UdpSendContext : public UdpContext
	{
	public:
		UdpBuffer buffer{};
		sockaddr_in remoteAddress{};
		int remoteAddressLength = static_cast<int>(sizeof(remoteAddress));

	public:
		UdpSendContext();

	public:
		void Prepare(const sockaddr_in& address, const char* data, int size) noexcept;
	};

	static_assert(offsetof(UdpContext, overlapped) == 0);
}