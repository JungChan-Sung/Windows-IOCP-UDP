#include <exception>
#include <iostream>

#include <Common/Net/WsaSession.h>

#include <Server/App/GameServerApp.h>

int main()
{
	try
	{
		// WinSock Lifetime이 모든 네트워크 객체보다 길도록 App 생성 전에 초기화
		common::net::WsaSession wsaSession;
		const common::net::WsaSession::InitializeResult initializeResult = wsaSession.Initialize();
		if (!initializeResult.has_value())
		{
			std::cerr << "WsaSession.Initialize failed. ErrorCode=" << initializeResult.error().errorCode << '\n';
			return 1;
		}

		server::app::GameServerApp gameServerApp;
		const server::app::GameServerApp::RunResult runResult = gameServerApp.Run();
		if (!runResult.has_value())
		{
			std::cerr << "GameServerApp.Run failed. Error=" << server::app::GameServerApp::ToString(runResult.error()) << '\n';
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