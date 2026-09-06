#include "ClientRuntime.h"

#include <cstdint>
#include <optional>
#include <thread>

#include <Common/Game/InputFlags.h>
#include <Common/Time/TimeTypes.h>

#include <Client/Config/ClientConfig.h>
#include <Client/Game/ClientWorld.h>
#include <Client/Input/InputSnapshot.h>
#include <Client/Net/AccountLoginState.h>
#include <Client/Net/UdpClient.h>
#include <Client/Ui/GameWindow.h>

namespace client::runtime
{
	bool ClientRuntime::Start(const config::ClientConfig& config, common::log::ILogger& logger, game::ClientWorld& world, net::UdpClient& udpClient, ui::GameWindow& gameWindow)
	{
		if (isRunning_.load())
		{
			return false;
		}

		config_ = &config;
		logger_ = &logger;
		world_ = &world;
		udpClient_ = &udpClient;
		gameWindow_ = &gameWindow;

		accountLoginFailed_.store(false);

		const common::time::TimePoint currentTime = common::time::Clock::now();

		joinHandshakeState_.Reset();

		nextSimulationTickTime_ = currentTime;
		nextKeepAliveTime_ = currentTime + config.timing.keepAliveInterval;
		nextRoomJoinTime_ = currentTime;
		lastEffectUpdateTime_ = currentTime;

		isRunning_.store(true);

		updateThread_ = std::jthread(
			[this](std::stop_token stopToken)
			{
				UpdateLoop(stopToken);
			}
		);

		return true;
	}

	void ClientRuntime::Stop()
	{
		isRunning_.store(false);

		if (updateThread_.joinable())
		{
			updateThread_.request_stop();
			updateThread_.join();
		}

		ShutdownJoinedSession();

		updateThread_ = std::jthread();
		joinHandshakeState_.Reset();

		config_ = nullptr;
		logger_ = nullptr;
		world_ = nullptr;
		udpClient_ = nullptr;
		gameWindow_ = nullptr;
	}

	void ClientRuntime::UpdateLoop(std::stop_token stopToken)
	{
		while (!stopToken.stop_requested() && isRunning_.load())
		{
			Update();
			std::this_thread::sleep_for(config_->timing.updateSleepInterval);
		}
	}

	bool ClientRuntime::ProcessAccountLogin(common::time::TimePoint currentTime)
	{
		if (joinHandshakeState_.GetState() != net::JoinHandshakeState::State::Idle)
		{
			return true;
		}

		udpClient_->ProcessAccountLogin();

		const net::UdpClient::AccountLoginSnapshot loginSnapshot = udpClient_->GetAccountLoginSnapshot();
		switch (loginSnapshot.state)
		{
		case net::AccountLoginState::State::Idle:
		case net::AccountLoginState::State::WaitingResponse:
			return false;

		case net::AccountLoginState::State::Succeeded:
			joinHandshakeState_.Begin(currentTime, config_->timing.joinRetryInterval);
			logger_->Info("Account login completed. Starting join handshake.");
			return true;

		case net::AccountLoginState::State::Failed:
			if (!accountLoginFailed_.exchange(true))
			{
				logger_->Error("Account login failed. Closing client.");
				isRunning_.store(false);
				gameWindow_->RequestClose();
			}

			return false;

		default:
			return false;
		}
	}

	bool ClientRuntime::TryBeginRecovery(common::time::TimePoint currentTime)
	{
		if (!world_->IsJoined())
		{
			return false;
		}

		const common::time::Duration serverSilenceTimeout = config_->timing.keepAliveInterval * serverSilenceKeepAliveMultiplier;
		if (!udpClient_->HasServerReceiveTimedOut(currentTime, serverSilenceTimeout))
		{
			return false;
		}

		logger_->Warning("Server packet receive timeout detected. Starting session recovery.");

		world_->BeginRecovery();
		joinHandshakeState_.Begin(currentTime, config_->timing.joinRetryInterval);

		const net::UdpClient::StartResult restartResult = udpClient_->RestartTransport(config_->network.serverIp.c_str(), config_->network.serverPort);
		if (!restartResult.has_value())
		{
			logger_->Error("UDP transport restart failed during session recovery.");
			isRunning_.store(false);
			gameWindow_->RequestClose();
			return true;
		}

		nextSimulationTickTime_ = currentTime + config_->simulation.tickInterval;
		nextKeepAliveTime_ = currentTime + config_->timing.keepAliveInterval;
		nextRoomJoinTime_ = currentTime + config_->timing.roomJoinInterval;

		logger_->Info("UDP transport restarted. Starting recovery join handshake.");

		return true;
	}

	void ClientRuntime::Update()
	{
		if (udpClient_->HasReceivedServerDisconnect())
		{
			isRunning_.store(false);
			gameWindow_->RequestClose();
			return;
		}

		const input::InputSnapshot inputSnapshot = gameWindow_->ConsumeInputSnapshot();
		TryToggleInterpolation(inputSnapshot);
		TryAdjustInterpolationDelay(inputSnapshot);

		const common::time::TimePoint currentTime = common::time::Clock::now();
		if (TryBeginRecovery(currentTime))
		{
			return;
		}

		if (!ProcessAccountLogin(currentTime))
		{
			return;
		}

		udpClient_->ProcessReliableResends();

		const float effectDeltaSeconds = common::time::FloatSeconds(currentTime - lastEffectUpdateTime_).count();
		lastEffectUpdateTime_ = currentTime;

		world_->UpdateLocalEffects(effectDeltaSeconds);

		if (!world_->IsJoined())
		{
			if (joinHandshakeState_.TryStartAttempt(currentTime))
			{
				udpClient_->SendJoinRequest();
			}

			return;
		}

		joinHandshakeState_.Complete();

		TrySendKeepAlive(currentTime);

		const bool isLocalPlayerDead = world_->IsLocalPlayerDead();
		const common::game::InputFlags inputFlags = inputSnapshot.movementFlags;

		int processedSimulationTickCount = 0;
		while (currentTime >= nextSimulationTickTime_ && processedSimulationTickCount < maxSimulationTicksPerUpdate)
		{
			if (!isLocalPlayerDead)
			{
				const std::optional<std::uint32_t> inputSequence = udpClient_->SendInputCommand(inputFlags);
				if (inputSequence.has_value())
				{
					world_->ApplyLocalPredictionTick(*inputSequence, inputFlags, config_->simulation.deltaSeconds);
				}
			}

			nextSimulationTickTime_ += config_->simulation.tickInterval;
			++processedSimulationTickCount;
		}

		if (processedSimulationTickCount == maxSimulationTicksPerUpdate && currentTime >= nextSimulationTickTime_)
		{
			nextSimulationTickTime_ = currentTime + config_->simulation.tickInterval;
		}

		if (currentTime >= nextRoomJoinTime_)
		{
			TryJoinRoom(inputSnapshot);
			nextRoomJoinTime_ = currentTime + config_->timing.roomJoinInterval;
		}

		if (inputSnapshot.fireRequested && !isLocalPlayerDead)
		{
			udpClient_->SendFireRequest();
		}
	}

	void ClientRuntime::TrySendKeepAlive(common::time::TimePoint currentTime)
	{
		if (currentTime < nextKeepAliveTime_)
		{
			return;
		}

		if (!udpClient_->SendKeepAlive())
		{
			logger_->Warning("Keep-alive packet send failed.");
		}

		nextKeepAliveTime_ = currentTime + config_->timing.keepAliveInterval;
	}

	void ClientRuntime::TryJoinRoom(const input::InputSnapshot& inputSnapshot) noexcept
	{
		if (world_->IsLocalPlayerDead())
		{
			return;
		}

		if (inputSnapshot.isRoom1Pressed)
		{
			udpClient_->SendJoinRoomRequest(1);
		}

		if (inputSnapshot.isRoom2Pressed)
		{
			udpClient_->SendJoinRoomRequest(2);
		}

		if (inputSnapshot.isRoom3Pressed)
		{
			udpClient_->SendJoinRoomRequest(3);
		}
	}

	void ClientRuntime::TryAdjustInterpolationDelay(const input::InputSnapshot& inputSnapshot) noexcept
	{
		if (inputSnapshot.decreaseInterpolationRequested)
		{
			world_->SetInterpolationDelay(world_->GetInterpolationDelay() - config_->timing.interpolationAdjustStep);
		}

		if (inputSnapshot.increaseInterpolationRequested)
		{
			world_->SetInterpolationDelay(world_->GetInterpolationDelay() + config_->timing.interpolationAdjustStep);
		}
	}

	void ClientRuntime::TryToggleInterpolation(const input::InputSnapshot& inputSnapshot)
	{
		if (!inputSnapshot.toggleInterpolationRequested)
		{
			return;
		}

		const bool isEnabled = world_->ToggleInterpolationEnabled();

		logger_->Info(isEnabled ? "Remote player interpolation enabled." : "Remote player interpolation disabled.");
	}

	void ClientRuntime::ShutdownJoinedSession()
	{
		if (!world_->IsJoined() || udpClient_->HasReceivedServerDisconnect())
		{
			return;
		}

		constexpr common::time::Milliseconds leaveResponseTimeout{ 1500 };
		constexpr common::time::Milliseconds leaveResponsePollInterval{ 10 };

		bool leaveRequestQueued = udpClient_->SendLeaveRequest();
		const common::time::TimePoint deadline = common::time::Clock::now() + leaveResponseTimeout;
		while (!udpClient_->HasReceivedLeaveResponse() && common::time::Clock::now() < deadline)
		{
			udpClient_->ProcessReliableResends();

			if (!leaveRequestQueued)
			{
				leaveRequestQueued = udpClient_->SendLeaveRequest();
			}

			std::this_thread::sleep_for(leaveResponsePollInterval);
		}

		if (!leaveRequestQueued)
		{
			logger_->Warning("Failed to queue leave request before timeout.");
		}
		else if (!udpClient_->HasReceivedLeaveResponse())
		{
			logger_->Warning("Leave response timed out.");
		}
	}
}