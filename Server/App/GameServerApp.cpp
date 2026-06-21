#include "GameServerApp.h"

#include <Windows.h>

#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>

#include <Common/String/StringFormat.h>

#include <Server/Config/ServerConfigLoader.h>

namespace server::app
{
	std::string GameServerApp::ToString(const RunError& runError)
	{
		return std::visit(
			[](const auto& error) -> std::string
			{
				using ErrorType = std::remove_cvref_t<decltype(error)>;

				if constexpr (std::is_same_v<ErrorType, common::log::AsyncLogWriter::StartError>)
				{
					return common::string::FormatScopedName("Logger", common::log::AsyncLogWriter::ToString(error));
				}
				else if constexpr (std::is_same_v<ErrorType, net::UdpServer::StartError>)
				{
					return common::string::FormatScopedName("UdpServer", net::UdpServer::ToString(error));
				}
				else
				{
					return "Unknown";
				}
			},
			runError
		);
	}

	GameServerApp::RunResult GameServerApp::Run(unsigned short port)
	{
		const config::ServerConfigLoadResult loadResult = BuildServerConfig(port);

		logger_.SetMinimumLogLevel(loadResult.config.diagnostics.logLevel);

		const common::log::AsyncLogWriter::StartResult loggerStartResult = logger_.Start(
			loadResult.config.diagnostics.asyncLogWorkerThreadCount
		);
		if (!loggerStartResult.has_value())
		{
			return std::unexpected(RunError{ loggerStartResult.error() });
		}

		LogConfigWarnings(loadResult.warningList);

		udpServer_.AttachLogger(logger_);

		const net::UdpServer::StartResult udpServerStartResult = udpServer_.Start(loadResult.config);
		if (!udpServerStartResult.has_value())
		{
			udpServer_.DetachLogger();
			logger_.Stop();
			return std::unexpected(RunError{ udpServerStartResult.error() });
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
		config::ServerConfigLoadResult loadResult = config::ServerConfigLoader::LoadValidated("Server.ini");

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
		const auto tickIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(serverConfig.tick.tickInterval).count();

		const auto resendIntervalMs = serverConfig.reliableUdp.resendInterval.count();
		const auto faultMinDelayMs = serverConfig.udpFaultSimulation.minDelay.count();
		const auto faultMaxDelayMs = serverConfig.udpFaultSimulation.maxDelay.count();
		const auto faultReorderDelayMs = serverConfig.udpFaultSimulation.reorderDelay.count();

		std::ostringstream stream;
		stream << "UDP game server started. "
			<< "Port=" << serverConfig.network.port
			<< ", WorkerThreadCount=" << serverConfig.network.workerThreadCount
			<< ", RecvContextCount=" << serverConfig.network.recvContextCount
			<< ", InitialRoomId=" << serverConfig.session.initialRoomId
			<< ", PeerTimeoutSeconds=" << serverConfig.session.peerTimeout.count()
			<< ", ReliableMaxPendingPacketCount=" << serverConfig.reliableUdp.maxPendingPacketCount
			<< ", ReliableMaxResendCount=" << serverConfig.reliableUdp.maxResendCount
			<< ", ReliableResendIntervalMs=" << resendIntervalMs
			<< ", UdpFaultEnabled=" << std::boolalpha << serverConfig.udpFaultSimulation.enabled
			<< ", UdpFaultDropRate=" << serverConfig.udpFaultSimulation.dropRate
			<< ", UdpFaultDuplicateRate=" << serverConfig.udpFaultSimulation.duplicateRate
			<< ", UdpFaultReorderRate=" << serverConfig.udpFaultSimulation.reorderRate
			<< ", UdpFaultMinDelayMs=" << faultMinDelayMs
			<< ", UdpFaultMaxDelayMs=" << faultMaxDelayMs
			<< ", UdpFaultReorderDelayMs=" << faultReorderDelayMs
			<< ", UdpFaultRandomSeed=" << serverConfig.udpFaultSimulation.randomSeed
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
			<< ", EnableStatusLog=" << serverConfig.diagnostics.enableStatusLog
			<< ", StatusLogIntervalSeconds=" << serverConfig.diagnostics.statusLogInterval.count()
			<< ", LogLevel=" << common::log::ToString(serverConfig.diagnostics.logLevel)
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