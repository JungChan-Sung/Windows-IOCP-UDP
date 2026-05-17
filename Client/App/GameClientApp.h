#pragma once

#include <WinSock2.h>
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <Client/Config/ClientConfig.h>
#include <Client/Game/ClientWorld.h>
#include <Client/Net/UdpClient.h>
#include <Client/Render/GdiRenderer.h>
#include <Client/Ui/GameWindow.h>

namespace client::app
{
	class GameClientApp
	{
	private:
		config::ClientConfig config_;

		game::ClientWorld world_;
		net::UdpClient udpClient_;
		ui::GameWindow gameWindow_;
		render::GdiRenderer gdiRenderer_;

		std::atomic<bool> isRunning_ = false;
		std::jthread updateThread_;

		std::chrono::steady_clock::time_point nextJoinRetryTime_;
		std::chrono::steady_clock::time_point nextSimulationTickTime_;
		std::chrono::steady_clock::time_point nextRoomJoinTime_;
		std::chrono::steady_clock::time_point lastEffectUpdateTime_;

	public:
		GameClientApp() = default;
		~GameClientApp() noexcept = default;

		GameClientApp(const GameClientApp&) = delete;
		GameClientApp& operator=(const GameClientApp&) = delete;

		GameClientApp(GameClientApp&&) = delete;
		GameClientApp& operator=(GameClientApp&&) = delete;

	public:
		[[nodiscard]] bool Run(HINSTANCE instanceHandle, const char* serverIp, unsigned short serverPort);

	private:
		[[nodiscard]] config::ClientConfig BuildClientConfig(const char* serverIp, unsigned short serverPort) const;

		void OutputStartupConfig() const;

		int MessageLoop();
		void UpdateLoop(std::stop_token stopToken);
		void Update();
		void TryJoinRoom() noexcept;
		void TryAdjustInterpolationDelay() noexcept;
	};
}

