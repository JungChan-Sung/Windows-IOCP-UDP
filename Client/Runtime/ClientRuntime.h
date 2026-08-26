#pragma once

#include <atomic>
#include <thread>

#include <Common/Log/ILogger.h>
#include <Common/Time/TimeTypes.h>

#include <Client/Net/JoinHandshakeState.h>

namespace client::config
{
	struct ClientConfig;
}

namespace client::game
{
	class ClientWorld;
}

namespace client::input
{
	struct InputSnapshot;
}

namespace client::net
{
	class UdpClient;
}

namespace client::ui
{
	class GameWindow;
}

namespace client::runtime
{
	class ClientRuntime
	{
	private:
		static inline constexpr int maxSimulationTicksPerUpdate = 4;

	private:
		const config::ClientConfig* config_ = nullptr;
		common::log::ILogger* logger_ = nullptr;
		game::ClientWorld* world_ = nullptr;
		net::UdpClient* udpClient_ = nullptr;
		ui::GameWindow* gameWindow_ = nullptr;

		net::JoinHandshakeState joinHandshakeState_;

		std::atomic<bool> isRunning_ = false;
		std::atomic<bool> accountLoginFailed_ = false;

		std::jthread updateThread_;

		common::time::TimePoint nextSimulationTickTime_;
		common::time::TimePoint nextKeepAliveTime_;
		common::time::TimePoint nextRoomJoinTime_;
		common::time::TimePoint lastEffectUpdateTime_;

	public:
		ClientRuntime() = default;
		~ClientRuntime() noexcept = default;

		ClientRuntime(const ClientRuntime&) = delete;
		ClientRuntime& operator=(const ClientRuntime&) = delete;

		ClientRuntime(ClientRuntime&&) = delete;
		ClientRuntime& operator=(ClientRuntime&&) = delete;

	public:
		[[nodiscard]] bool Start(
			const config::ClientConfig& config,
			common::log::ILogger& logger,
			game::ClientWorld& world,
			net::UdpClient& udpClient,
			ui::GameWindow& gameWindow
		);
		void Stop();

	private:
		void UpdateLoop(std::stop_token stopToken);
		void Update();

		[[nodiscard]] bool ProcessAccountLogin(common::time::TimePoint currentTime);

		void TrySendKeepAlive(common::time::TimePoint currentTime);
		void TryJoinRoom(const input::InputSnapshot& inputSnapshot) noexcept;
		void TryAdjustInterpolationDelay(const input::InputSnapshot& inputSnapshot) noexcept;

		void ShutdownJoinedSession();

	public:
		[[nodiscard]] bool IsRunning() const noexcept
		{
			return isRunning_.load();
		}

		[[nodiscard]] bool HasAccountLoginFailed() const noexcept
		{
			return accountLoginFailed_.load();
		}
	};
}