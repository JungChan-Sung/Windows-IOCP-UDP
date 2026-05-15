#include "GameServerApp.h"

#include <Windows.h>

#include <chrono>
#include <sstream>
#include <thread>
#include <string_view>

#include <Server/Config/ServerConfigLoader.h>

namespace
{
	[[nodiscard]] std::string_view ToString(common::log::LogLevel logLevel) noexcept
	{
		switch (logLevel)
		{
		case common::log::LogLevel::Trace:
			return "Trace";

		case common::log::LogLevel::Debug:
			return "Debug";

		case common::log::LogLevel::Info:
			return "Info";

		case common::log::LogLevel::Warning:
			return "Warning";

		case common::log::LogLevel::Error:
			return "Error";

		default:
			return "Unknown";
		}
	}
}

namespace server::app
{
	bool GameServerApp::Run(unsigned short port)
	{
		const config::ServerConfigLoadResult loadResult = BuildServerConfig(port);

		logger_.SetMinimumLogLevel(loadResult.config.diagnostics.logLevel);

		if (!logger_.Start(loadResult.config.diagnostics.asyncLogWorkerThreadCount))
		{
			return false;
		}

		LogConfigWarnings(loadResult.warningList);

		udpServer_.AttachLogger(logger_);

		if (!udpServer_.Start(loadResult.config))
		{
			logger_.Error("Failed to start UDP game server.");
			udpServer_.DetachLogger();
			logger_.Stop();
			return false;
		}

		LogStartupConfig(udpServer_.GetConfig());

		MainLoop();

		udpServer_.Stop();
		udpServer_.DetachLogger();

		logger_.Stop();

		return true;
	}

	config::ServerConfigLoadResult GameServerApp::BuildServerConfig(unsigned short port) const
	{
		config::ServerConfigLoadResult loadResult = config::ServerConfigLoader::Load("Server.ini");

		if (port != 0)
		{
			loadResult.config.network.port = port;
		}

		return loadResult;
	}

	void GameServerApp::LogConfigWarnings(std::span<const config::ServerConfigWarning> warningList) const
	{
		for (const config::ServerConfigWarning& warning : warningList)
		{
			if (warning.lineNumber == 0)
			{
				logger_.Warning(warning.message);
				continue;
			}

			std::ostringstream stream;
			stream << "Server.ini:" << warning.lineNumber << ": " << warning.message;
			logger_.Warning(stream.str());
		}
	}

	void GameServerApp::LogStartupConfig(const config::ServerConfig& serverConfig) const
	{
		const auto tickIntervalMs =
			std::chrono::duration_cast<std::chrono::milliseconds>(serverConfig.tick.tickInterval).count();

		std::ostringstream stream;
		stream << "UDP game server started. "
			<< "Port=" << serverConfig.network.port
			<< ", WorkerThreadCount=" << serverConfig.network.workerThreadCount
			<< ", RecvContextCount=" << serverConfig.network.recvContextCount
			<< ", InitialRoomId=" << serverConfig.session.initialRoomId
			<< ", PeerTimeoutSeconds=" << serverConfig.session.peerTimeout.count()
			<< ", TickIntervalMs=" << tickIntervalMs
			<< ", FixedDeltaSeconds=" << serverConfig.tick.fixedDeltaSeconds
			<< ", InitialPlayerHp=" << serverConfig.gameRule.initialPlayerHp
			<< ", RespawnDelaySeconds=" << serverConfig.gameRule.respawnDelaySeconds
			<< ", RespawnInvincibilitySeconds=" << serverConfig.gameRule.respawnInvincibilitySeconds
			<< ", HitFlashDurationSeconds=" << serverConfig.gameRule.hitFlashDurationSeconds
			<< ", BasicBulletDamage=" << serverConfig.weaponRule.basicWeaponRule.bulletDamage
			<< ", BasicBulletSpeed=" << serverConfig.weaponRule.basicWeaponRule.bulletSpeed
			<< ", BasicBulletLifeSeconds=" << serverConfig.weaponRule.basicWeaponRule.bulletLifeSeconds
			<< ", BasicBulletRadius=" << serverConfig.weaponRule.basicWeaponRule.bulletRadius
			<< ", BasicFireCooldownSeconds=" << serverConfig.weaponRule.basicWeaponRule.fireCooldownSeconds
			<< ", EnableStatusLog=" << std::boolalpha << serverConfig.diagnostics.enableStatusLog
			<< ", StatusLogIntervalSeconds=" << serverConfig.diagnostics.statusLogInterval.count()
			<< ", LogLevel=" << ToString(serverConfig.diagnostics.logLevel)
			<< ", AsyncLogWorkerThreadCount=" << serverConfig.diagnostics.asyncLogWorkerThreadCount;

		logger_.Info(stream.str());
		logger_.Info("Press ESC to stop.");
	}

	void GameServerApp::MainLoop() noexcept
	{
		using namespace std::chrono_literals;

		while (true)
		{
			if ((::GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
			{
				break;
			}

			std::this_thread::sleep_for(10ms);
		}
	}
}