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
		const common::net::WsaSession::InitializeResult initializeResult = wsaSession.Initialize();
		if (!initializeResult.has_value())
		{
			std::cerr << "WsaSession.Initialize failed. ErrorCode=" << initializeResult.error().errorCode << '\n';
			return 1;
		}

		HINSTANCE instanceHandle = ::GetModuleHandleW(nullptr);
		if (instanceHandle == nullptr)
		{
			std::cerr << "GetModuleHandleW failed.\n";
			return 1;
		}

		client::app::GameClientApp gameClientApp;
		const client::app::GameClientApp::RunResult runResult = gameClientApp.Run(instanceHandle);
		if (!runResult.has_value())
		{
			std::cerr << "GameClientApp.Run failed. Error=" << client::app::GameClientApp::ToString(runResult.error()) << '\n';
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