#pragma once

#include <span>

#include <Common/Log/ConsoleLogger.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Config/ServerConfigWarning.h>
#include <Server/Net/UdpServer.h>

namespace server::app
{
	class GameServerApp
	{
	private:
		common::log::ConsoleLogger logger_;
		net::UdpServer udpServer_;

	public:
		GameServerApp() = default;
		~GameServerApp() noexcept = default;

		GameServerApp(const GameServerApp&) = delete;
		GameServerApp& operator=(const GameServerApp&) = delete;

		GameServerApp(GameServerApp&&) = delete;
		GameServerApp& operator=(GameServerApp&&) = delete;

	public:
		[[nodiscard]] bool Run(unsigned short port);

	private:
		[[nodiscard]] config::ServerConfig BuildServerConfig(unsigned short port) const;

		void LogConfigWarnings(std::span<const config::ServerConfigWarning> warningList) const;
		void LogStartupConfig(const config::ServerConfig& serverConfig) const;

		void MainLoop() noexcept;
	};
}

