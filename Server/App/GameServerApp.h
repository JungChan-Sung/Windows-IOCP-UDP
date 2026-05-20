#pragma once

#include <expected>
#include <span>
#include <string_view>

#include <Common/Log/AsyncLogWriter.h>

#include <Server/Config/ServerConfigLoader.h>
#include <Server/Net/UdpServer.h>

namespace server::app
{
	class GameServerApp
	{
	public:
		enum class RunError
		{
			LoggerStartFailed,

			UdpServerAlreadyRunning,
			UdpServerTransportStartFailed,
			UdpServerGameTickRunnerStartFailed,
		};

	public:
		using RunResult = std::expected<void, RunError>;

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
		[[nodiscard]] static std::string_view ToString(RunError runError) noexcept;

	private:
		[[nodiscard]] static RunError ToRunError(net::UdpServer::StartError startError) noexcept;

	public:
		[[nodiscard]] RunResult Run(unsigned short port = 0);

	private:
		[[nodiscard]] config::ServerConfigLoadResult BuildServerConfig(unsigned short port) const;

		void LogConfigWarnings(std::span<const config::ServerConfigWarning> warningList) const;
		void LogStartupConfig(const config::ServerConfig& serverConfig) const;

		void MainLoop() noexcept;
	};
}

