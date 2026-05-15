#include "GameServerApp.h"

#include <Windows.h>

#include <chrono>
#include <sstream>
#include <thread>

#include <Server/Config/ServerConfigLoader.h>

namespace server::app
{
	bool GameServerApp::Run(unsigned short port)
	{
		if (!logger_.Start(1))
		{
			return false;
		}

		udpServer_.AttachLogger(logger_);

		const config::ServerConfig serverConfig = BuildServerConfig(port);

		if (!udpServer_.Start(serverConfig))
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

	config::ServerConfig GameServerApp::BuildServerConfig(unsigned short port) const
	{
		config::ServerConfigLoadResult loadResult = config::ServerConfigLoader::Load("Server.ini");
		LogConfigWarnings(loadResult.warningList);

		config::ServerConfig serverConfig = loadResult.config;

		if (port != 0)
		{
			serverConfig.network.port = port;
		}

		return serverConfig;
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
			<< ", StatusLogIntervalSeconds=" << serverConfig.diagnostics.statusLogInterval.count();

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