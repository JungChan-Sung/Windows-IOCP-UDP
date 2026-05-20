#include "GameServerApp.h"

#include <Windows.h>

#include <chrono>
#include <sstream>
#include <string_view>
#include <thread>

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
	std::string_view GameServerApp::ToString(RunError runError) noexcept
	{
		switch (runError)
		{
		case RunError::LoggerStartFailed:
			return "LoggerStartFailed";

		case RunError::UdpServerAlreadyRunning:
			return "UdpServerAlreadyRunning";

		case RunError::UdpServerTransportAlreadyRunning:
			return "UdpServerTransportAlreadyRunning";

		case RunError::UdpServerTransportInvalidPacketReceivedHandler:
			return "UdpServerTransportInvalidPacketReceivedHandler";

		case RunError::UdpServerTransportCreateSocketFailed:
			return "UdpServerTransportCreateSocketFailed";

		case RunError::UdpServerTransportBindSocketFailed:
			return "UdpServerTransportBindSocketFailed";

		case RunError::UdpServerTransportConfigureSocketFailed:
			return "UdpServerTransportConfigureSocketFailed";

		case RunError::UdpServerTransportCreateIocpFailed:
			return "UdpServerTransportCreateIocpFailed";

		case RunError::UdpServerTransportStartWorkerThreadsFailed:
			return "UdpServerTransportStartWorkerThreadsFailed";

		case RunError::UdpServerTransportCreateRecvContextsFailed:
			return "UdpServerTransportCreateRecvContextsFailed";

		case RunError::UdpServerGameTickRunnerStartFailed:
			return "UdpServerGameTickRunnerStartFailed";

		default:
			return "Unknown";
		}
	}

	GameServerApp::RunError GameServerApp::ToRunError(net::UdpServer::StartError startError) noexcept
	{
		switch (startError)
		{
		case net::UdpServer::StartError::AlreadyRunning:
			return RunError::UdpServerAlreadyRunning;

		case net::UdpServer::StartError::UdpTransportAlreadyRunning:
			return RunError::UdpServerTransportAlreadyRunning;

		case net::UdpServer::StartError::UdpTransportInvalidPacketReceivedHandler:
			return RunError::UdpServerTransportInvalidPacketReceivedHandler;

		case net::UdpServer::StartError::UdpTransportCreateSocketFailed:
			return RunError::UdpServerTransportCreateSocketFailed;

		case net::UdpServer::StartError::UdpTransportBindSocketFailed:
			return RunError::UdpServerTransportBindSocketFailed;

		case net::UdpServer::StartError::UdpTransportConfigureSocketFailed:
			return RunError::UdpServerTransportConfigureSocketFailed;

		case net::UdpServer::StartError::UdpTransportCreateIocpFailed:
			return RunError::UdpServerTransportCreateIocpFailed;

		case net::UdpServer::StartError::UdpTransportStartWorkerThreadsFailed:
			return RunError::UdpServerTransportStartWorkerThreadsFailed;

		case net::UdpServer::StartError::UdpTransportCreateRecvContextsFailed:
			return RunError::UdpServerTransportCreateRecvContextsFailed;

		case net::UdpServer::StartError::GameTickRunnerStartFailed:
			return RunError::UdpServerGameTickRunnerStartFailed;

		default:
			return RunError::UdpServerTransportCreateRecvContextsFailed;
		}
	}

	GameServerApp::RunResult GameServerApp::Run(unsigned short port)
	{
		const config::ServerConfigLoadResult loadResult = BuildServerConfig(port);

		logger_.SetMinimumLogLevel(loadResult.config.diagnostics.logLevel);

		if (!logger_.Start(loadResult.config.diagnostics.asyncLogWorkerThreadCount))
		{
			return std::unexpected(RunError::LoggerStartFailed);
		}

		LogConfigWarnings(loadResult.warningList);

		udpServer_.AttachLogger(logger_);

		const net::UdpServer::StartResult udpServerStartResult = udpServer_.Start(loadResult.config);
		if (!udpServerStartResult.has_value())
		{
			logger_.Error("Failed to start UDP game server.");
			udpServer_.DetachLogger();
			logger_.Stop();
			return std::unexpected(ToRunError(udpServerStartResult.error()));
		}

		LogStartupConfig(udpServer_.GetConfig());

		MainLoop();

		udpServer_.Stop();
		udpServer_.DetachLogger();

		logger_.Stop();

		return {};
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
			<< ", LogLevel=" << ::ToString(serverConfig.diagnostics.logLevel)
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