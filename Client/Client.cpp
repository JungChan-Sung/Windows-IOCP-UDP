#include <WinSock2.h>
#include <Windows.h>

#include <exception>
#include <iostream>

#include <Common/Net/WsaSession.h>

#include <Client/App/GameClientApp.h>

int main()
{
	try
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
		if (!gameClientApp.Run(instanceHandle))
		{
			std::cerr << "GameClientApp.Run failed.\n";
			return 1;
		}

		return 0;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "Unhandled exception: " << exception.what() << '\n';
		return 1;
	}
	catch (...)
	{
		std::cerr << "Unhandled unknown exception.\n";
		return 1;
	}
}