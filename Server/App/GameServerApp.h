#pragma once

#include <expected>
#include <span>
#include <string>
#include <variant>

#include <Common/Log/AsyncLogWriter.h>

#include <Persistence/Core/DatabaseError.h>

#include <Server/Config/ServerConfigLoader.h>
#include <Server/Net/UdpServer.h>

namespace server::app
{
	class GameServerApp
	{
	public:
		using RunError = std::variant<
			common::log::AsyncLogWriter::StartError,
			net::UdpServer::StartError,
			persistence::core::DatabaseError
		>;
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
		[[nodiscard]] static std::string ToString(const RunError& runError);

	public:
		[[nodiscard]] RunResult Run(unsigned short port = 0);

	private:
		[[nodiscard]] config::ServerConfigLoadResult BuildServerConfig(unsigned short port) const;

		void LogConfigWarnings(std::span<const config::ServerConfigWarning> warningList) const;
		void LogStartupConfig(const config::ServerConfig& serverConfig) const;

		void MainLoop() noexcept;
	};
}

