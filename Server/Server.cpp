#include <iostream>
#include <string>

#include <Common/Net/WsaSession.h>

#include <Server/App/GameServerApp.h>

int main()
{
	common::net::WsaSession wsaSession;
	if (!wsaSession.Initialize())
	{
		return 1;
	}

	server::app::GameServerApp gameServerApp;
	if (!gameServerApp.Run(9000))
	{
		return 1;
	}

	return 0;
}