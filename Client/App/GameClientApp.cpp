#include "GameClientApp.h"

#include <Windows.h>

#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include <Common/Log/AsyncLogWriterGuard.h>
#include <Common/Log/LogMessageBuilder.h>
#include <Common/Packet/Account/AccountPacket.h>
#include <Common/String/StringFormat.h>

#include <Client/Config/ClientConfigLoader.h>
#include <Client/Config/ClientTransportType.h>

namespace client::app
{
	std::string GameClientApp::ToString(const RunError& runError)
	{
		return std::visit(
			[](const auto& error) -> std::string
			{
				using ErrorType = std::remove_cvref_t<decltype(error)>;

				if constexpr (std::is_same_v<ErrorType, RunFailure>)
				{
					switch (error)
					{
					case RunFailure::AlreadyRunning:
						return "AlreadyRunning";

					case RunFailure::AccountLoginStartFailed:
						return "AccountLoginStartFailed";

					case RunFailure::AccountLoginFailed:
						return "AccountLoginFailed";

					case RunFailure::GameWindowCreateFailed:
						return "GameWindowCreateFailed";

					case RunFailure::RuntimeStartFailed:
						return "RuntimeStartFailed";

					case RunFailure::MessageLoopFailed:
						return "MessageLoopFailed";

					default:
						return "Unknown";
					}
				}
				else if constexpr (std::is_same_v<ErrorType, net::UdpClient::StartError>)
				{
					return common::string::FormatScopedName("UdpClient", net::UdpClient::ToString(error));
				}
				else if constexpr (std::is_same_v<ErrorType, common::log::AsyncLogWriter::StartError>)
				{
					return common::string::FormatScopedName("Logger", common::log::AsyncLogWriter::ToString(error));
				}
				else
				{
					return "Unknown";
				}
			},
			runError
		);
	}

	GameClientApp::RunResult GameClientApp::Run(HINSTANCE instanceHandle, const char* serverIp, unsigned short serverPort)
	{
		if (isRunning_.load())
		{
			return std::unexpected(RunError{ RunFailure::AlreadyRunning });
		}

		const config::ClientConfigLoadResult loadResult = BuildClientConfig(serverIp, serverPort);
		config_ = loadResult.config;

		debugOutputLogger_.SetMinimumLogLevel(config_.diagnostics.logLevel);

		logger_.SetLogger(debugOutputLogger_);
		logger_.SetMinimumLogLevel(config_.diagnostics.logLevel);

		const common::log::AsyncLogWriter::StartResult loggerStartResult = logger_.Start(config_.diagnostics.asyncLogWorkerThreadCount);
		if (!loggerStartResult.has_value())
		{
			return std::unexpected(RunError{ loggerStartResult.error() });
		}

		common::log::AsyncLogWriterGuard loggerGuard(logger_);

		udpClient_.AttachLogger(logger_);

		LogConfigWarnings(loadResult.warningList);
		OutputStartupConfig();

		world_.SetInterpolationSettings(config_.interpolation.defaultDelay, config_.interpolation.minDelay, config_.interpolation.maxDelay);

		udpClient_.SetSnapshotAssemblyTimeout(config_.snapshot.assemblyTimeout);
		udpClient_.SetTransportConfig(config_.network.transportType, config_.network.iocpWorkerThreadCount, config_.network.iocpRecvContextCount);

		const net::UdpClient::StartResult udpClientStartResult = udpClient_.Start(
			config_.network.serverIp.c_str(),
			config_.network.serverPort,
			world_
		);
		if (!udpClientStartResult.has_value())
		{
			udpClient_.DetachLogger();

			return std::unexpected(RunError{ udpClientStartResult.error() });
		}

		const net::UdpClient::AccountLoginRequestId accountLoginRequestId = udpClient_.BeginAccountLogin(
			config_.account.loginName,
			config_.account.passwordHash,
			config_.timing.accountLoginRetryInterval
		);
		if (accountLoginRequestId == common::packet::invalidAccountLoginRequestId)
		{
			udpClient_.Stop();
			udpClient_.DetachLogger();
			world_.Clear();

			return std::unexpected(RunError{ RunFailure::AccountLoginStartFailed });
		}

		if (!gameWindow_.Create(instanceHandle, world_, gdiRenderer_, L"UDP Game Client"))
		{
			udpClient_.Stop();
			udpClient_.DetachLogger();
			world_.Clear();

			return std::unexpected(RunError{ RunFailure::GameWindowCreateFailed });
		}

		isRunning_.store(true);

		if (!runtime_.Start(config_, logger_, world_, udpClient_, gameWindow_))
		{
			isRunning_.store(false);

			gameWindow_.Destroy();
			udpClient_.Stop();
			udpClient_.DetachLogger();
			world_.Clear();

			return std::unexpected(RunError{ RunFailure::RuntimeStartFailed });
		}

		const int exitCode = MessageLoop();

		isRunning_.store(false);

		runtime_.Stop();

		gameWindow_.Destroy();
		udpClient_.Stop();
		udpClient_.DetachLogger();
		world_.Clear();

		if (exitCode != 0)
		{
			return std::unexpected(RunError{ RunFailure::MessageLoopFailed });
		}

		if (runtime_.HasAccountLoginFailed())
		{
			return std::unexpected(RunError{ RunFailure::AccountLoginFailed });
		}

		return {};
	}

	config::ClientConfigLoadResult GameClientApp::BuildClientConfig(const char* serverIp, unsigned short serverPort) const
	{
		config::ClientConfigLoadResult loadResult = config::ClientConfigLoader::LoadValidated("Client.ini");

		if (serverIp != nullptr && serverIp[0] != '\0')
		{
			loadResult.config.network.serverIp = serverIp;
		}

		if (serverPort != 0)
		{
			loadResult.config.network.serverPort = serverPort;
		}

		return loadResult;
	}

	void GameClientApp::LogConfigWarnings(std::span<const config::ClientConfigWarning> warningList) const
	{
		for (const config::ClientConfigWarning& warning : warningList)
		{
			if (warning.lineNumber == 0)
			{
				logger_.Warning(warning.message);
				continue;
			}

			const std::string message = common::log::LogMessageBuilder{}
				.Append("Client.ini:")
				.Append(warning.lineNumber)
				.Append(": ")
				.Append(warning.message)
				.Build();

			logger_.Warning(message);
		}
	}

	void GameClientApp::OutputStartupConfig() const
	{
		const bool isAccountLoginConfigured = !config_.account.loginName.empty() && !config_.account.passwordHash.empty();

		const std::string message = common::log::LogMessageBuilder{}
			.Append("Client config. ")
			.AppendNamedValue("ServerIp", config_.network.serverIp)
			.AppendCommaNamedValue("ServerPort", config_.network.serverPort)
			.AppendCommaNamedValue("AccountLoginConfigured", isAccountLoginConfigured)
			.AppendCommaNamedValue("AccountLoginRetryMs", config_.timing.accountLoginRetryInterval.count())
			.AppendCommaNamedValue("TransportType", config::ToString(config_.network.transportType))
			.AppendCommaNamedValue("IocpWorkerThreadCount", config_.network.iocpWorkerThreadCount)
			.AppendCommaNamedValue("IocpRecvContextCount", config_.network.iocpRecvContextCount)
			.AppendCommaNamedValue("UpdateSleepMs", config_.timing.updateSleepInterval.count())
			.AppendCommaNamedValue("JoinRetryMs", config_.timing.joinRetryInterval.count())
			.AppendCommaNamedValue("KeepAliveMs", config_.timing.keepAliveInterval.count())
			.AppendCommaNamedValue("RoomJoinMs", config_.timing.roomJoinInterval.count())
			.AppendCommaNamedValue("InterpolationAdjustStepMs", config_.timing.interpolationAdjustStep.count())
			.AppendCommaNamedValue("InterpolationDefaultDelayMs", config_.interpolation.defaultDelay.count())
			.AppendCommaNamedValue("InterpolationMinDelayMs", config_.interpolation.minDelay.count())
			.AppendCommaNamedValue("InterpolationMaxDelayMs", config_.interpolation.maxDelay.count())
			.AppendCommaNamedValue("SnapshotAssemblyTimeoutMs", config_.snapshot.assemblyTimeout.count())
			.AppendCommaNamedValue("SimulationTickIntervalMs", config_.simulation.tickInterval.count())
			.AppendCommaNamedValue("SimulationDeltaSeconds", config_.simulation.deltaSeconds)
			.AppendCommaNamedValue("LogLevel", common::log::ToString(config_.diagnostics.logLevel))
			.AppendCommaNamedValue("AsyncLogWorkerThreadCount", config_.diagnostics.asyncLogWorkerThreadCount)
			.Build();

		logger_.Info(message);
	}

	int GameClientApp::MessageLoop()
	{
		MSG message{};

		while (isRunning_.load())
		{
			const BOOL result = ::GetMessageW(&message, nullptr, 0, 0);

			if (result == 0)
			{
				return static_cast<int>(message.wParam);
			}

			if (result == -1)
			{
				return -1;
			}

			::TranslateMessage(&message);
			::DispatchMessageW(&message);
		}

		return 0;
	}
}