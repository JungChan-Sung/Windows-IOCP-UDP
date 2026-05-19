#include <exception>
#include <iostream>

#include <Common/Net/WsaSession.h>

#include <Server/App/GameServerApp.h>

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

		server::app::GameServerApp gameServerApp;
		if (!gameServerApp.Run(9000))
		{
			std::cerr << "GameServerApp.Run failed.\n";
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