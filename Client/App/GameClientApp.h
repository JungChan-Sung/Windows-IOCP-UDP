#pragma once

#include <WinSock2.h>
#include <Windows.h>

#include <atomic>
#include <expected>
#include <span>
#include <string>
#include <thread>
#include <variant>

#include <Common/Log/AsyncLogWriter.h>
#include <Common/Log/DebugOutputLogger.h>
#include <Common/Time/TimeTypes.h>

#include <Client/Config/ClientConfig.h>
#include <Client/Config/ClientConfigLoader.h>
#include <Client/Game/ClientWorld.h>
#include <Client/Net/JoinHandshakeState.h>
#include <Client/Net/UdpClient.h>
#include <Client/Render/GdiRenderer.h>
#include <Client/Ui/GameWindow.h>

namespace client::app
{
	class GameClientApp
	{
	public:
		enum class RunFailure
		{
			AlreadyRunning,
			GameWindowCreateFailed,
			MessageLoopFailed,
		};

	public:
		using RunError = std::variant<RunFailure, common::log::AsyncLogWriter::StartError, net::UdpClient::StartError>;
		using RunResult = std::expected<void, RunError>;

	private:
		static inline constexpr int maxSimulationTicksPerUpdate = 4;

	private:
		config::ClientConfig config_;

		common::log::DebugOutputLogger debugOutputLogger_;
		common::log::AsyncLogWriter logger_;

		game::ClientWorld world_;
		net::UdpClient udpClient_;
		net::JoinHandshakeState joinHandshakeState_;
		ui::GameWindow gameWindow_;
		render::GdiRenderer gdiRenderer_;

		std::atomic<bool> isRunning_ = false;
		std::jthread updateThread_;

		common::time::TimePoint nextSimulationTickTime_;
		common::time::TimePoint nextRoomJoinTime_;
		common::time::TimePoint lastEffectUpdateTime_;

	public:
		GameClientApp() = default;
		~GameClientApp() noexcept = default;

		GameClientApp(const GameClientApp&) = delete;
		GameClientApp& operator=(const GameClientApp&) = delete;

		GameClientApp(GameClientApp&&) = delete;
		GameClientApp& operator=(GameClientApp&&) = delete;

	public:
		[[nodiscard]] static std::string ToString(const RunError& runError);

	public:
		[[nodiscard]] RunResult Run(HINSTANCE instanceHandle, const char* serverIp = nullptr, unsigned short serverPort = 0);

	private:
		[[nodiscard]] config::ClientConfigLoadResult BuildClientConfig(const char* serverIp, unsigned short serverPort) const;

		void LogConfigWarnings(std::span<const config::ClientConfigWarning> warningList) const;
		void OutputStartupConfig() const;

		int MessageLoop();
		void UpdateLoop(std::stop_token stopToken);
		void Update();
		void TryJoinRoom() noexcept;
		void TryAdjustInterpolationDelay() noexcept;
	};
}

