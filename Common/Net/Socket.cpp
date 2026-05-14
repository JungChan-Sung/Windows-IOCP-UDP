#include "Socket.h"

#include <utility>

namespace common::net
{
	Socket::Socket(SOCKET handle) noexcept
		: handle_(handle)
	{
	}

	Socket::~Socket() noexcept
	{
		Close();
	}

	Socket::Socket(Socket&& other) noexcept
		: handle_(std::exchange(other.handle_, INVALID_SOCKET))
	{
	}

	Socket& Socket::operator=(Socket&& other) noexcept
	{
		if (this != &other)
		{
			Close();
			handle_ = std::exchange(other.handle_, INVALID_SOCKET);
		}

		return *this;
	}

	void Socket::Reset(SOCKET handle) noexcept
	{
		if (handle_ == handle)
		{
			return;
		}

		Close();
		handle_ = handle;
	}

	void Socket::Close() noexcept
	{
		if (handle_ != INVALID_SOCKET)
		{
			::closesocket(handle_);
			handle_ = INVALID_SOCKET;
		}
	}
}
