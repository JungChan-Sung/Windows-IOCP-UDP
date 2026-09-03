#pragma once

#include <WinSock2.h>

namespace common::net
{
	// WinSock SOCKET - 소유권 및 수명 관리 클래스
	class Socket
	{
	private:
		SOCKET handle_ = INVALID_SOCKET;

	public:
		Socket() = default;
		explicit Socket(SOCKET handle) noexcept;
		~Socket() noexcept;

		Socket(const Socket&) = delete;
		Socket& operator=(const Socket&) = delete;

		Socket(Socket&& other) noexcept;
		Socket& operator=(Socket&& other) noexcept;

	public:
		void Reset(SOCKET handle = INVALID_SOCKET) noexcept;
		void Close() noexcept;

	public:
		[[nodiscard]] SOCKET Get() const noexcept
		{
			return handle_;
		}

		[[nodiscard]] bool IsValid() const noexcept
		{
			return handle_ != INVALID_SOCKET;
		}

		[[nodiscard]] SOCKET Release() noexcept
		{
			SOCKET handle = handle_;
			handle_ = INVALID_SOCKET;
			return handle;
		}
	};
}