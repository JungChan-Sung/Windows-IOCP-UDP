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

	WsaSession::InitializeResult WsaSession::Initialize() noexcept
	{
		if (isInitialized_)
		{
			return {};
		}

		WSADATA wsaData{};
		const int result = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (result != 0)
		{
			return std::unexpected(InitializeError{ .errorCode = result });
		}

		isInitialized_ = true;
		return {};
	}
}