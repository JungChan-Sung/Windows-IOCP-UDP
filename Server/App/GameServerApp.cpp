#include "GameServerApp.h"

#include <chrono>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

#include <Common/String/StringFormat.h>
#include <Common/Log/AsyncLogWriterGuard.h>
#include <Common/Log/LogMessageBuilder.h>
#include <Common/Time/TimeTypes.h>

#include <Persistence/Core/DatabaseError.h>

#include <Server/Admin/ServerAdminCommand.h>
#include <Server/Config/ServerConfigDatabaseApplier.h>
#include <Server/Config/ServerConfigLoader.h>
#include <Server/Config/ServerConfigValidator.h>
#include <Server/Diagnostics/ServerStatusReporter.h>

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
		config::ServerConfigLoadResult loadResult = BuildServerConfig(port);

		std::size_t databaseConfigEntryCount = 0;
		std::size_t databaseConfigAppliedCount = 0;

		const persistence::PersistenceRuntime::StartResult persistenceStartResult = persistenceRuntime_.Start(persistence::PersistenceRuntimeStartConfig{
			.enabled = loadResult.config.database.enabled,
			.connectionString = loadResult.config.database.connectionString,
			.connectionTimeoutSeconds = loadResult.config.database.connectionTimeoutSeconds,
			});
		if (!persistenceStartResult.has_value())
		{
			return std::unexpected(RunError{ persistenceStartResult.error() });
		}

		if (persistenceRuntime_.IsStarted())
		{
			persistence::PersistenceRuntime::LoadServerConfigEntriesResult configEntryResult = persistenceRuntime_.LoadServerConfigEntries();
			if (!configEntryResult.has_value())
			{
				persistenceRuntime_.Stop();

				return std::unexpected(RunError{ configEntryResult.error() });
			}

			databaseConfigEntryCount = configEntryResult->size();
			config::ServerConfigDatabaseApplyResult applyResult = config::ServerConfigDatabaseApplier::Apply(loadResult.config, *configEntryResult);
			databaseConfigAppliedCount = applyResult.appliedCount;
			for (config::ServerConfigWarning& warning : applyResult.warningList)
			{
				loadResult.warningList.push_back(std::move(warning));
			}
		}

		std::vector<config::ServerConfigWarning> validationWarningList = config::ServerConfigValidator::ValidateAndNormalize(loadResult.config);
		for (config::ServerConfigWarning& warning : validationWarningList)
		{
			loadResult.warningList.push_back(std::move(warning));
		}

		logger_.SetMinimumLogLevel(loadResult.config.diagnostics.logLevel);
		const common::log::AsyncLogWriter::StartResult loggerStartResult = logger_.Start(loadResult.config.diagnostics.asyncLogWorkerThreadCount);
		if (!loggerStartResult.has_value())
		{
			persistenceRuntime_.Stop();

			return std::unexpected(RunError{ loggerStartResult.error() });
		}

		common::log::AsyncLogWriterGuard loggerGuard(logger_);

		LogConfigWarnings(loadResult.warningList);

		if (persistenceRuntime_.IsStarted())
		{
			logger_.Info("Database health check succeeded.");

			const std::string message
				= common::log::LogMessageBuilder{}
				.Append("Database config loaded. ")
				.AppendNamedValue("EntryCount", databaseConfigEntryCount)
				.AppendCommaNamedValue("AppliedCount", databaseConfigAppliedCount)
				.Build();

			logger_.Info(message);
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
		logger_.Info("Type 'help' for admin commands. Press ESC to stop.");
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

	bool GameServerApp::ProcessAdminCommand(std::string_view commandLine)
	{
		const admin::ServerAdminCommand command = admin::ParseServerAdminCommand(commandLine);
		switch (command.type)
		{
		case admin::ServerAdminCommandType::Help:
			logger_.Info("Admin commands: help, status, players, rooms, kick <playerId>, stop.");
			return false;

		case admin::ServerAdminCommandType::Status:
		{
			const diagnostics::ServerStatusSnapshot snapshot = udpServer_.CaptureStatusSnapshot();
			logger_.Info(diagnostics::ServerStatusReporter::BuildMessage(snapshot));
			return false;
		}

		case admin::ServerAdminCommandType::Players:
		{
			const diagnostics::ServerDetailSnapshot snapshot = udpServer_.CaptureDetailSnapshot();
			if (snapshot.playerList.empty())
			{
				logger_.Info("Players: none.");
				return false;
			}

			{
				std::ostringstream stream;
				stream << "Players: " << snapshot.playerList.size();
				logger_.Info(stream.str());
			}

			for (const diagnostics::ServerPlayerDetailSnapshot& player : snapshot.playerList)
			{
				std::ostringstream stream;
				stream << "PlayerId=" << player.playerId
					<< ", AccountId=" << player.accountId
					<< ", PersistentPlayerId=" << player.persistentPlayerId
					<< ", Nickname=" << player.nickname
					<< ", RoomId=" << player.roomId
					<< ", AcceptedInput=" << player.lastAcceptedInputSequence
					<< ", ProcessedInput=" << player.lastProcessedInputSequence;
				logger_.Info(stream.str());
			}

			return false;
		}

		case admin::ServerAdminCommandType::Rooms:
		{
			const diagnostics::ServerDetailSnapshot snapshot = udpServer_.CaptureDetailSnapshot();
			if (snapshot.roomList.empty())
			{
				logger_.Info("Rooms: none.");
				return false;
			}

			{
				std::ostringstream stream;
				stream << "Rooms: " << snapshot.roomList.size();
				logger_.Info(stream.str());
			}

			for (const diagnostics::ServerRoomDetailSnapshot& room : snapshot.roomList)
			{
				std::ostringstream stream;
				stream << "RoomId=" << room.roomId << ", Members=" << room.memberCount;
				logger_.Info(stream.str());
			}

			return false;
		}

		case admin::ServerAdminCommandType::Kick:
		{
			const net::UdpServer::KickPlayerResult kickResult = udpServer_.KickPlayer(command.playerId);
			if (!kickResult.kicked)
			{
				std::ostringstream stream;
				stream << "Kick failed. PlayerId=" << command.playerId << " was not found.";

				logger_.Warning(stream.str());
				return false;
			}

			std::ostringstream stream;
			stream << "Kick completed. PlayerId=" << kickResult.playerId
				<< ", RoomId=" << kickResult.roomId
				<< ", DisconnectNotificationQueued=" << kickResult.disconnectNotificationQueued;
			logger_.Info(stream.str());
			return false;
		}

		case admin::ServerAdminCommandType::Stop:
			logger_.Info("Server stop requested by admin command.");
			return true;

		case admin::ServerAdminCommandType::Unknown:
		default:
			logger_.Warning("Unknown admin command. Type 'help' for available commands.");
			return false;
		}
	}

	void GameServerApp::MainLoop()
	{
		while (true)
		{
			ProcessCompletedMatches();
			ProcessMatchHistorySaveCompletions();

			const admin::ServerAdminConsole::Event consoleEvent = adminConsole_.Poll();
			if (consoleEvent.type == admin::ServerAdminConsole::EventType::StopRequested)
			{
				logger_.Info("Server stop requested by ESC.");
				break;
			}

			if (consoleEvent.type == admin::ServerAdminConsole::EventType::CommandLine && ProcessAdminCommand(consoleEvent.commandLine))
			{
				break;
			}

			std::this_thread::sleep_for(common::time::Milliseconds(10));
		}
	}
}