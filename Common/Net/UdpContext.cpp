#include "UdpContext.h"

#include <cstring>

namespace common::net
{
	UdpRecvContext::UdpRecvContext()
	{
		operationType = UdpOperationType::Recv;
		wsaBuffer.buf = buffer.data();
		wsaBuffer.len = static_cast<ULONG>(buffer.size());
	}

	void UdpRecvContext::Reset() noexcept
	{
		overlapped = {};
		remoteAddress = {};
		remoteAddressLength = static_cast<int>(sizeof(remoteAddress));
		flags = 0;
		wsaBuffer.buf = buffer.data();
		wsaBuffer.len = static_cast<ULONG>(buffer.size());
	}

	UdpSendContext::UdpSendContext()
	{
		operationType = UdpOperationType::Send;
		wsaBuffer.buf = buffer.data();
		wsaBuffer.len = 0;
	}

	void UdpSendContext::Prepare(const sockaddr_in& address, const char* data, int size) noexcept
	{
		overlapped = {};
		remoteAddress = address;
		remoteAddressLength = static_cast<int>(sizeof(remoteAddress));

		if (size < 0)
		{
			size = 0;
		}

		if (size > static_cast<int>(buffer.size()))
		{
			size = static_cast<int>(buffer.size());
		}

		if (size > 0 && data != nullptr)
		{
			std::memcpy(buffer.data(), data, static_cast<std::size_t>(size));
		}

		wsaBuffer.buf = buffer.data();
		wsaBuffer.len = static_cast<ULONG>(size);
	}
}