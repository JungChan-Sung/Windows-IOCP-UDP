#include <WinSock2.h>
#include <Windows.h>

#include <iostream>
#include <string>

#include <Common/Net/WsaSession.h>

#include <Client/App/GameClientApp.h>

int main()
{
	common::net::WsaSession wsaSession;
	if (!wsaSession.Initialize())
	{
		std::cerr << "WsaSession.Initialize failed.\n";
		return 1;
	}

	HINSTANCE instanceHandle = ::GetModuleHandleW(nullptr);
	if (instanceHandle == nullptr)
	{
		std::cerr << "GetModuleHandleW failed.\n";
		return 1;
	}

	client::app::GameClientApp gameClientApp;
	if (!gameClientApp.Run(instanceHandle, "127.0.0.1", 9000))
	{
		std::cerr << "GameClientApp.Run failed.\n";
		return 1;
	}

	return 0;
}