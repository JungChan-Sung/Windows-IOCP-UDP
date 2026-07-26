#pragma once

#include <expected>
#include <span>
#include <string>
#include <variant>

#include <Common/Log/AsyncLogWriter.h>
#include <Common/Threading/ThreadPool.h>

#include <Persistence/Core/PersistenceRuntime.h>

#include <Server/Account/AccountLoginTaskProcessor.h>
#include <Server/Account/AccountService.h>
#include <Server/Config/ServerConfigLoader.h>
#include <Server/Net/AccountLoginPacketHandler.h>
#include <Server/Net/UdpServer.h>

namespace server::app
{
	class GameServerApp
	{
	public:
		using RunError = std::variant<
			common::log::AsyncLogWriter::StartError,
			common::threading::ThreadPool::StartError,
			net::UdpServer::StartError,
			persistence::core::DatabaseError
		>;
		using RunResult = std::expected<void, RunError>;

	private:
		common::log::AsyncLogWriter logger_;

		persistence::PersistenceRuntime persistenceRuntime_;

		account::AccountService accountService_;
		account::AccountLoginTaskProcessor accountLoginTaskProcessor_;

		net::AccountLoginPacketHandler accountLoginPacketHandler_;
		net::UdpServer udpServer_;

	public:
		GameServerApp();
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

