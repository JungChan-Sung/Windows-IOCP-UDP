#include "GameClientApp.h"

#include <Windows.h>

#include <sstream>
#include <span>
#include <string>
#include <vector>

#include <Client/Config/ClientConfigLoader.h>
#include <Client/Config/ClientConfigValidator.h>

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
}

namespace client::app
{
	bool GameClientApp::Run(HINSTANCE instanceHandle, const char* serverIp, unsigned short serverPort)
	{
		if (isRunning_.load())
		{
			return false;
		}

		config_ = BuildClientConfig(serverIp, serverPort);

		world_.SetInterpolationSettings(
			config_.interpolation.defaultDelay,
			config_.interpolation.minDelay,
			config_.interpolation.maxDelay
		);

		udpClient_.SetSnapshotAssemblyTimeout(config_.snapshot.assemblyTimeout);
		udpClient_.SetEnableChunkAssemblerDebugTests(config_.diagnostics.enableChunkAssemblerDebugTests);

		if (!udpClient_.Start(config_.network.serverIp.c_str(), config_.network.serverPort, world_))
		{
			return false;
		}

		if (!gameWindow_.Create(instanceHandle, world_, gdiRenderer_, L"UDP Game Client"))
		{
			udpClient_.Stop();
			world_.Clear();
			return false;
		}

		isRunning_.store(true);

		const auto currentTime = std::chrono::steady_clock::now();
		nextJoinRetryTime_ = currentTime;
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

		gameWindow_.Destroy();
		udpClient_.Stop();
		world_.Clear();

		return exitCode == 0;
	}

	config::ClientConfig GameClientApp::BuildClientConfig(const char* serverIp, unsigned short serverPort) const
	{
		config::ClientConfigLoadResult loadResult = config::ClientConfigLoader::Load("Client.ini");
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

		const std::vector<config::ClientConfigWarning> validationWarningList
			= config::ClientConfigValidator::ValidateAndNormalize(clientConfig);
		OutputClientConfigWarnings(validationWarningList);

		return clientConfig;
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

		const float effectDeltaSeconds = std::chrono::duration<float>(currentTime - lastEffectUpdateTime_).count();
		lastEffectUpdateTime_ = currentTime;

		world_.UpdateLocalEffects(effectDeltaSeconds);

		if (!world_.IsJoined())
		{
			if (currentTime >= nextJoinRetryTime_)
			{
				udpClient_.SendJoinRequest();
				nextJoinRetryTime_ = currentTime + config_.timing.joinRetryInterval;
			}

			return;
		}

		while (currentTime >= nextSimulationTickTime_)
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
				world_.ApplyLocalPredictionTick(inputSequence, inputFlags, config_.simulation.deltaSeconds);
			}

			nextSimulationTickTime_ += config_.simulation.tickInterval;
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