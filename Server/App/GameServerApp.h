#pragma once

#include <span>

#include <Common/Log/AsyncLogWriter.h>

#include <Server/Config/ServerConfigLoader.h>
#include <Server/Net/UdpServer.h>

namespace server::app
{
	class GameServerApp
	{
	private:
		common::log::AsyncLogWriter logger_;
		net::UdpServer udpServer_;

	public:
		GameServerApp() = default;
		~GameServerApp() noexcept = default;

		GameServerApp(const GameServerApp&) = delete;
		GameServerApp& operator=(const GameServerApp&) = delete;

		GameServerApp(GameServerApp&&) = delete;
		GameServerApp& operator=(GameServerApp&&) = delete;

	public:
		[[nodiscard]] bool Run(unsigned short port = 0);

	private:
		[[nodiscard]] config::ServerConfigLoadResult BuildServerConfig(unsigned short port) const;

		void LogConfigWarnings(std::span<const config::ServerConfigWarning> warningList) const;
		void LogStartupConfig(const config::ServerConfig& serverConfig) const;

		void MainLoop() noexcept;
	};
}

