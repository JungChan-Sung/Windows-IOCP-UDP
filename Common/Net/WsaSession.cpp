#include "WsaSession.h"

#include <WinSock2.h>

namespace common::net
{
	WsaSession::~WsaSession() noexcept
	{
		if (isInitialized_)
		{
			::WSACleanup();
			isInitialized_ = false;
		}
	}

	bool WsaSession::Initialize() noexcept
	{
		if (isInitialized_)
		{
			return true;
		}

		WSADATA wsaData{};
		const int result = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (result != 0)
		{
			return false;
		}

		isInitialized_ = true;
		return true;
	}
}