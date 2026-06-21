#include "GameClientApp.h"

#include <Windows.h>

#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include <Common/String/StringFormat.h>

#include <Client/Config/ClientConfigLoader.h>
#include <Client/Config/ClientTransportType.h>

namespace
{
	void OutputClientConfigWarnings(std::span<const client::config::ClientConfigWarning> warningList)
	{
		for (const client::config::ClientConfigWarning& warning : warningList)
		{
			std::ostringstream stream;

			if (warning.lineNumber == 0)
			{
				stream << warning.message << '\n';
			}
			else
			{
				stream << "Client.ini:" << warning.lineNumber << ": " << warning.message << '\n';
			}

			::OutputDebugStringA(stream.str().c_str());
		}
	}

	[[nodiscard]] std::string_view ToString(client::config::ClientTransportType transportType) noexcept
	{
		switch (transportType)
		{
		case client::config::ClientTransportType::Socket:
			return "Socket";

		case client::config::ClientTransportType::Iocp:
			return "Iocp";

		default:
			return "Unknown";
		}
	}
}

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

					case RunFailure::GameWindowCreateFailed:
						return "GameWindowCreateFailed";

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

		config_ = BuildClientConfig(serverIp, serverPort);
		OutputStartupConfig();

		world_.SetInterpolationSettings(
			config_.interpolation.defaultDelay,
			config_.interpolation.minDelay,
			config_.interpolation.maxDelay
		);

		udpClient_.SetSnapshotAssemblyTimeout(config_.snapshot.assemblyTimeout);
		udpClient_.SetEnableChunkAssemblerDebugTests(config_.diagnostics.enableChunkAssemblerDebugTests);
		udpClient_.SetTransportConfig(
			config_.network.transportType,
			config_.network.iocpWorkerThreadCount,
			config_.network.iocpRecvContextCount
		);

		const net::UdpClient::StartResult udpClientStartResult = udpClient_.Start(config_.network.serverIp.c_str(), config_.network.serverPort, world_);
		if (!udpClientStartResult.has_value())
		{
			return std::unexpected(RunError{ udpClientStartResult.error() });
		}

		if (!gameWindow_.Create(instanceHandle, world_, gdiRenderer_, L"UDP Game Client"))
		{
			udpClient_.Stop();
			world_.Clear();
			return std::unexpected(RunError{ RunFailure::GameWindowCreateFailed });
		}

		isRunning_.store(true);

		const auto currentTime = std::chrono::steady_clock::now();

		joinHandshakeState_.Begin(
			currentTime,
			config_.timing.joinRetryInterval
		);

		nextSimulationTickTime_ = currentTime;
		nextRoomJoinTime_ = currentTime;
		lastEffectUpdateTime_ = currentTime;

		updateThread_ = std::jthread(
			[this](std::stop_token stopToken)
			{
				UpdateLoop(stopToken);
			}
		);

		const int exitCode = MessageLoop();

		isRunning_.store(false);

		if (updateThread_.joinable())
		{
			updateThread_.request_stop();
			updateThread_.join();
		}

		if (world_.IsJoined())
		{
			udpClient_.SendLeaveRequest();
		}

		updateThread_ = std::jthread();
		joinHandshakeState_.Reset();

		gameWindow_.Destroy();
		udpClient_.Stop();
		world_.Clear();

		if (exitCode != 0)
		{
			return std::unexpected(RunError{ RunFailure::MessageLoopFailed });
		}

		return {};
	}

	config::ClientConfig GameClientApp::BuildClientConfig(const char* serverIp, unsigned short serverPort) const
	{
		config::ClientConfigLoadResult loadResult = config::ClientConfigLoader::LoadValidated("Client.ini");
		OutputClientConfigWarnings(loadResult.warningList);

		config::ClientConfig clientConfig = loadResult.config;

		if (serverIp != nullptr && serverIp[0] != '\0')
		{
			clientConfig.network.serverIp = serverIp;
		}

		if (serverPort != 0)
		{
			clientConfig.network.serverPort = serverPort;
		}

		return clientConfig;
	}

	void GameClientApp::OutputStartupConfig() const
	{
		std::ostringstream stream;

		stream << "Client config. "
			<< "ServerIp=" << config_.network.serverIp
			<< ", ServerPort=" << config_.network.serverPort
			<< ", TransportType=" << ::ToString(config_.network.transportType)
			<< ", IocpWorkerThreadCount=" << config_.network.iocpWorkerThreadCount
			<< ", IocpRecvContextCount=" << config_.network.iocpRecvContextCount
			<< ", UpdateSleepMs=" << config_.timing.updateSleepInterval.count()
			<< ", JoinRetryMs=" << config_.timing.joinRetryInterval.count()
			<< ", RoomJoinMs=" << config_.timing.roomJoinInterval.count()
			<< ", InterpolationDelayMs=" << config_.interpolation.defaultDelay.count()
			<< ", SnapshotAssemblyTimeoutMs=" << config_.snapshot.assemblyTimeout.count()
			<< ", SimulationTickIntervalMs=" << config_.simulation.tickInterval.count()
			<< ", SimulationDeltaSeconds=" << config_.simulation.deltaSeconds
			<< ", EnableChunkAssemblerDebugTests=" << std::boolalpha << config_.diagnostics.enableChunkAssemblerDebugTests
			<< '\n';

		::OutputDebugStringA(stream.str().c_str());
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

	void GameClientApp::UpdateLoop(std::stop_token stopToken)
	{
		while (!stopToken.stop_requested() && isRunning_.load())
		{
			Update();
			std::this_thread::sleep_for(config_.timing.updateSleepInterval);
		}
	}

	void GameClientApp::Update()
	{
		TryAdjustInterpolationDelay();

		const auto currentTime = std::chrono::steady_clock::now();

		udpClient_.ProcessReliableResends();

		const float effectDeltaSeconds = std::chrono::duration<float>(currentTime - lastEffectUpdateTime_).count();
		lastEffectUpdateTime_ = currentTime;

		world_.UpdateLocalEffects(effectDeltaSeconds);

		if (!world_.IsJoined())
		{
			if (joinHandshakeState_.TryStartAttempt(currentTime))
			{
				udpClient_.SendJoinRequest();
			}

			return;
		}

		joinHandshakeState_.Complete();

		int processedSimulationTickCount = 0;
		while (currentTime >= nextSimulationTickTime_ && processedSimulationTickCount < maxSimulationTicksPerUpdate)
		{
			common::game::InputFlags inputFlags = common::game::InputFlags::None;

			if (!world_.IsLocalPlayerDead())
			{
				inputFlags = gameWindow_.GetInputState().ToInputFlags();
			}

			std::uint32_t inputSequence = 0;
			const bool sendResult = udpClient_.SendInputCommand(inputFlags, inputSequence);

			if (sendResult)
			{
				const float predictionDeltaSeconds = config_.simulation.deltaSeconds;
				world_.ApplyLocalPredictionTick(inputSequence, inputFlags, predictionDeltaSeconds);
			}

			nextSimulationTickTime_ += config_.simulation.tickInterval;
			++processedSimulationTickCount;
		}

		if (processedSimulationTickCount == maxSimulationTicksPerUpdate && currentTime >= nextSimulationTickTime_)
		{
			nextSimulationTickTime_ = currentTime + config_.simulation.tickInterval;
		}

		if (currentTime >= nextRoomJoinTime_)
		{
			TryJoinRoom();
			nextRoomJoinTime_ = currentTime + config_.timing.roomJoinInterval;
		}

		if (::GetForegroundWindow() == gameWindow_.GetWindowHandle() && !world_.IsLocalPlayerDead())
		{
			if ((::GetAsyncKeyState(VK_SPACE) & 0x001) != 0)
			{
				udpClient_.SendFireRequest();
			}
		}
	}

	void GameClientApp::TryJoinRoom() noexcept
	{
		if (::GetForegroundWindow() != gameWindow_.GetWindowHandle())
		{
			return;
		}

		if (world_.IsLocalPlayerDead())
		{
			return;
		}

		if ((::GetAsyncKeyState('1') & 0x8000) != 0)
		{
			udpClient_.SendJoinRoomRequest(1);
		}

		if ((::GetAsyncKeyState('2') & 0x8000) != 0)
		{
			udpClient_.SendJoinRoomRequest(2);
		}

		if ((::GetAsyncKeyState('3') & 0x8000) != 0)
		{
			udpClient_.SendJoinRoomRequest(3);
		}
	}

	void GameClientApp::TryAdjustInterpolationDelay() noexcept
	{
		if (::GetForegroundWindow() != gameWindow_.GetWindowHandle())
		{
			return;
		}

		const bool isDecreasePressed
			= ((::GetAsyncKeyState(VK_OEM_MINUS) & 0x0001) != 0)
			|| ((::GetAsyncKeyState(VK_SUBTRACT) & 0x0001) != 0);

		const bool isIncreasePressed
			= ((::GetAsyncKeyState(VK_OEM_PLUS) & 0x0001) != 0)
			|| ((::GetAsyncKeyState(VK_ADD) & 0x0001) != 0);

		if (isDecreasePressed)
		{
			world_.SetInterpolationDelay(world_.GetInterpolationDelay() - config_.timing.interpolationAdjustStep);
		}

		if (isIncreasePressed)
		{
			world_.SetInterpolationDelay(world_.GetInterpolationDelay() + config_.timing.interpolationAdjustStep);
		}
	}
}