#include "GameServerApp.h"

#include <Windows.h>

#include <chrono>
#include <expected>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>

#include <Common/String/StringFormat.h>
#include <Common/Log/AsyncLogWriterGuard.h>
#include <Common/Log/LogMessageBuilder.h>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>
#include <Persistence/Odbc/OdbcEnvironment.h>

#include <Server/Config/ServerConfigLoader.h>

namespace
{
	using DatabaseStartupResult = std::expected<void, persistence::core::DatabaseError>;

	[[nodiscard]] DatabaseStartupResult StartDatabaseIfEnabled(
		const server::config::DatabaseConfig& databaseConfig,
		persistence::odbc::OdbcEnvironment& databaseEnvironment,
		persistence::odbc::OdbcConnection& databaseConnection,
		const common::log::AsyncLogWriter& logger
	)
	{
		if (!databaseConfig.enabled)
		{
			logger.Info("Database connection is disabled.");
			return {};
		}

		if (databaseConfig.connectionString.empty())
		{
			return std::unexpected(persistence::core::DatabaseError{
				.failure = persistence::core::DatabaseFailure::ConnectionOpenFailed,
				.message = "Database connection string is empty.",
				});
		}

		const persistence::odbc::OdbcEnvironment::InitializeResult initializeResult = databaseEnvironment.Initialize();
		if (!initializeResult.has_value())
		{
			return std::unexpected(initializeResult.error());
		}

		const persistence::odbc::OdbcConnection::OpenResult openResult = databaseConnection.Open(
			databaseEnvironment,
			persistence::odbc::OdbcConnectionOpenConfig{
				.connectionString = databaseConfig.connectionString,
				.connectionTimeoutSeconds = databaseConfig.connectionTimeoutSeconds,
			}
			);
		if (!openResult.has_value())
		{
			return std::unexpected(openResult.error());
		}

		const persistence::odbc::OdbcConnection::HealthCheckResult healthCheckResult = databaseConnection.ExecuteHealthCheck();
		if (!healthCheckResult.has_value())
		{
			return std::unexpected(healthCheckResult.error());
		}

		logger.Info("Database health check succeeded.");
		return {};
	}
}

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
				else if constexpr (std::is_same_v<ErrorType, persistence::core::DatabaseError>)
				{
					return common::string::FormatScopedName("Database", persistence::core::ToString(error));
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

		common::log::AsyncLogWriterGuard loggerGuard(logger_);

		LogConfigWarnings(loadResult.warningList);

		persistence::odbc::OdbcEnvironment databaseEnvironment;
		persistence::odbc::OdbcConnection databaseConnection;

		const DatabaseStartupResult databaseStartupResult = StartDatabaseIfEnabled(
			loadResult.config.database,
			databaseEnvironment,
			databaseConnection,
			logger_
		);
		if (!databaseStartupResult.has_value())
		{
			logger_.Error(common::string::FormatScopedName("Database", persistence::core::ToString(databaseStartupResult.error())));
			return std::unexpected(RunError{ databaseStartupResult.error() });
		}

		udpServer_.AttachLogger(logger_);

		const net::UdpServer::StartResult udpServerStartResult = udpServer_.Start(loadResult.config);
		if (!udpServerStartResult.has_value())
		{
			udpServer_.DetachLogger();
			return std::unexpected(RunError{ udpServerStartResult.error() });
		}

		LogStartupConfig(udpServer_.GetConfig());

		MainLoop();

		udpServer_.Stop();
		udpServer_.DetachLogger();

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

			const std::string message =
				common::log::LogMessageBuilder{}
				.Append("Server.ini:")
				.Append(warning.lineNumber)
				.Append(": ")
				.Append(warning.message)
				.Build();

			logger_.Warning(message);
		}
	}

	void GameServerApp::LogStartupConfig(const config::ServerConfig& serverConfig) const
	{
		const auto tickIntervalMs = std::chrono::duration_cast<common::time::Milliseconds>(serverConfig.tick.tickInterval).count();

		const std::string message =
			common::log::LogMessageBuilder{}
			.Append("UDP game server started. ")
			.AppendNamedValue("Port", serverConfig.network.port)
			.AppendCommaNamedValue("WorkerThreadCount", serverConfig.network.workerThreadCount)
			.AppendCommaNamedValue("RecvContextCount", serverConfig.network.recvContextCount)
			.AppendCommaNamedValue("InitialRoomId", serverConfig.session.initialRoomId)
			.AppendCommaNamedValue("PeerTimeoutSeconds", serverConfig.session.peerTimeout.count())
			.AppendCommaNamedValue("ReliableMaxPendingPacketCount", serverConfig.reliableUdp.maxPendingPacketCount)
			.AppendCommaNamedValue("ReliableMaxResendCount", serverConfig.reliableUdp.maxResendCount)
			.AppendCommaNamedValue("ReliableResendIntervalMs", serverConfig.reliableUdp.resendInterval.count())
			.AppendCommaNamedValue("UdpFaultEnabled", serverConfig.udpFaultSimulation.enabled)
			.AppendCommaNamedValue("UdpFaultDropRate", serverConfig.udpFaultSimulation.dropRate)
			.AppendCommaNamedValue("UdpFaultDuplicateRate", serverConfig.udpFaultSimulation.duplicateRate)
			.AppendCommaNamedValue("UdpFaultReorderRate", serverConfig.udpFaultSimulation.reorderRate)
			.AppendCommaNamedValue("UdpFaultMinDelayMs", serverConfig.udpFaultSimulation.minDelay.count())
			.AppendCommaNamedValue("UdpFaultMaxDelayMs", serverConfig.udpFaultSimulation.maxDelay.count())
			.AppendCommaNamedValue("UdpFaultReorderDelayMs", serverConfig.udpFaultSimulation.reorderDelay.count())
			.AppendCommaNamedValue("UdpFaultRandomSeed", serverConfig.udpFaultSimulation.randomSeed)
			.AppendCommaNamedValue("TickIntervalMs", tickIntervalMs)
			.AppendCommaNamedValue("FixedDeltaSeconds", serverConfig.tick.fixedDeltaSeconds)
			.AppendCommaNamedValue("InitialPlayerHp", serverConfig.gameRule.initialPlayerHp)
			.AppendCommaNamedValue("RespawnDelaySeconds", serverConfig.gameRule.respawnDelaySeconds)
			.AppendCommaNamedValue("RespawnInvincibilitySeconds", serverConfig.gameRule.respawnInvincibilitySeconds)
			.AppendCommaNamedValue("HitFlashDurationSeconds", serverConfig.gameRule.hitFlashDurationSeconds)
			.AppendCommaNamedValue("BasicBulletDamage", serverConfig.weaponRule.basicWeaponRule.bulletDamage)
			.AppendCommaNamedValue("BasicBulletSpeed", serverConfig.weaponRule.basicWeaponRule.bulletSpeed)
			.AppendCommaNamedValue("BasicBulletLifeSeconds", serverConfig.weaponRule.basicWeaponRule.bulletLifeSeconds)
			.AppendCommaNamedValue("BasicBulletRadius", serverConfig.weaponRule.basicWeaponRule.bulletRadius)
			.AppendCommaNamedValue("BasicFireCooldownSeconds", serverConfig.weaponRule.basicWeaponRule.fireCooldownSeconds)
			.AppendCommaNamedValue("EnableStatusLog", serverConfig.diagnostics.enableStatusLog)
			.AppendCommaNamedValue("StatusLogIntervalSeconds", serverConfig.diagnostics.statusLogInterval.count())
			.AppendCommaNamedValue("LogLevel", common::log::ToString(serverConfig.diagnostics.logLevel))
			.AppendCommaNamedValue("AsyncLogWorkerThreadCount", serverConfig.diagnostics.asyncLogWorkerThreadCount)
			.Build();

		logger_.Info(message);
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