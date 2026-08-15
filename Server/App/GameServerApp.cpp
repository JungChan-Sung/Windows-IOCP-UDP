#include "GameServerApp.h"

#include <Windows.h>

#include <chrono>
#include <string>
#include <thread>
#include <type_traits>
#include <variant>

#include <Common/String/StringFormat.h>
#include <Common/Log/AsyncLogWriterGuard.h>
#include <Common/Log/LogMessageBuilder.h>
#include <Common/Time/TimeTypes.h>

#include <Persistence/Core/DatabaseError.h>

#include <Server/Config/ServerConfigLoader.h>

namespace server::app
{
	GameServerApp::GameServerApp()
		: accountService_(persistenceRuntime_),
		accountLoginTaskProcessor_(accountService_),
		matchHistoryTaskProcessor_(persistenceRuntime_),
		accountLoginPacketHandler_(accountLoginTaskProcessor_)
	{}

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
				else if constexpr (std::is_same_v<ErrorType, common::threading::ThreadPool::StartError>)
				{
					return common::string::FormatScopedName("ThreadPool", common::threading::ThreadPool::ToString(error));
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

		const persistence::PersistenceRuntime::StartResult persistenceStartResult = persistenceRuntime_.Start(
			persistence::PersistenceRuntimeStartConfig{
				.enabled = loadResult.config.database.enabled,
				.connectionString = loadResult.config.database.connectionString,
				.connectionTimeoutSeconds = loadResult.config.database.connectionTimeoutSeconds,
			});
		if (!persistenceStartResult.has_value())
		{
			logger_.Error(common::string::FormatScopedName("Database", persistence::core::ToString(persistenceStartResult.error())));
			return std::unexpected(RunError{ persistenceStartResult.error() });
		}

		if (persistenceRuntime_.IsStarted())
		{
			logger_.Info("Database health check succeeded.");
		}
		else
		{
			logger_.Info("Database connection is disabled.");
		}

		const account::AccountLoginTaskProcessor::StartResult accountLoginProcessorStartResult = accountLoginTaskProcessor_.Start();
		if (!accountLoginProcessorStartResult.has_value())
		{
			logger_.Error(common::string::FormatScopedName(
					"AccountLoginTaskProcessor",
					common::threading::ThreadPool::ToString(accountLoginProcessorStartResult.error())
				));

			persistenceRuntime_.Stop();

			return std::unexpected(RunError{ accountLoginProcessorStartResult.error() });
		}

		const match::MatchHistoryTaskProcessor::StartResult matchHistoryProcessorStartResult = matchHistoryTaskProcessor_.Start();
		if (!matchHistoryProcessorStartResult.has_value())
		{
			logger_.Error(common::string::FormatScopedName(
				"MatchHistoryTaskProcessor",
				common::threading::ThreadPool::ToString(matchHistoryProcessorStartResult.error())
			));

			accountLoginTaskProcessor_.Stop();
			accountLoginPacketHandler_.Clear();

			persistenceRuntime_.Stop();

			return std::unexpected(RunError{ matchHistoryProcessorStartResult.error() });
		}

		udpServer_.AttachLogger(logger_);
		udpServer_.AttachAccountLoginPacketHandler(accountLoginPacketHandler_);

		const net::UdpServer::StartResult udpServerStartResult = udpServer_.Start(loadResult.config);
		if (!udpServerStartResult.has_value())
		{
			udpServer_.DetachAccountLoginPacketHandler();
			udpServer_.DetachLogger();

			matchHistoryTaskProcessor_.Stop();

			accountLoginTaskProcessor_.Stop();
			accountLoginPacketHandler_.Clear();

			persistenceRuntime_.Stop();

			return std::unexpected(RunError{ udpServerStartResult.error() });
		}

		LogStartupConfig(udpServer_.GetConfig());

		MainLoop();

		udpServer_.Stop();

		ProcessCompletedMatches();

		matchHistoryTaskProcessor_.StopAfterDrain();
		ProcessMatchHistorySaveCompletions();

		udpServer_.DetachAccountLoginPacketHandler();
		udpServer_.DetachLogger();

		accountLoginTaskProcessor_.StopAfterDrain();
		accountLoginPacketHandler_.Clear();

		persistenceRuntime_.Stop();

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

	void GameServerApp::ProcessCompletedMatches()
	{
		game::CompletedMatchList completedMatchList = udpServer_.ExtractCompletedMatches();

		if (completedMatchList.empty())
		{
			return;
		}

		if (!persistenceRuntime_.IsEnabled())
		{
			return;
		}

		for (game::CompletedMatch& completedMatch : completedMatchList)
		{
			const common::game::RoomId roomId = completedMatch.roomId;

			if (matchHistoryTaskProcessor_.Enqueue(std::move(completedMatch)))
			{
				continue;
			}

			const std::string message = common::log::LogMessageBuilder{}
				.Append("Failed to enqueue completed match history. ")
				.AppendNamedValue("RoomId", roomId)
				.Build();

			logger_.Error(message);
		}
	}

	void GameServerApp::ProcessMatchHistorySaveCompletions()
	{
		match::MatchHistoryTaskProcessor::CompletionList completionList = matchHistoryTaskProcessor_.ExtractCompletionList();
		for (match::MatchHistorySaveCompletion& completion : completionList)
		{
			if (completion.saveResult.has_value())
			{
				const std::string message = common::log::LogMessageBuilder{}
					.Append("Match history saved. ")
					.AppendNamedValue("MatchId", *completion.saveResult)
					.AppendCommaNamedValue("RoomId", completion.roomId)
					.Build();

				logger_.Info(message);
				continue;
			}

			const std::string message = common::log::LogMessageBuilder{}
				.Append("Failed to save match history. ")
				.AppendNamedValue("RoomId", completion.roomId)
				.AppendCommaNamedValue("Error", persistence::core::ToString(completion.saveResult.error()))
				.Build();

			logger_.Error(message);
		}
	}

	void GameServerApp::MainLoop() noexcept
	{
		while (true)
		{
			ProcessCompletedMatches();
			ProcessMatchHistorySaveCompletions();

			if ((::GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
			{
				break;
			}

			std::this_thread::sleep_for(common::time::Milliseconds(10));
		}
	}
}