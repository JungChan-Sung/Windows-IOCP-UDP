#pragma once

#include <WinSock2.h>
#include <Windows.h>

#include <atomic>
#include <expected>
#include <span>
#include <string>
#include <variant>

#include <Common/Log/AsyncLogWriter.h>
#include <Common/Log/DebugOutputLogger.h>

#include <Client/Config/ClientConfig.h>
#include <Client/Config/ClientConfigLoader.h>
#include <Client/Game/ClientWorld.h>
#include <Client/Net/UdpClient.h>
#include <Client/Render/GdiRenderer.h>
#include <Client/Runtime/ClientRuntime.h>
#include <Client/Ui/GameWindow.h>

namespace client::app
{
	class GameClientApp
	{
	public:
		enum class RunFailure
		{
			AlreadyRunning,
			AccountLoginStartFailed,
			AccountLoginFailed,
			GameWindowCreateFailed,
			RuntimeStartFailed,
			MessageLoopFailed,
		};

	public:
		using RunError = std::variant<RunFailure, common::log::AsyncLogWriter::StartError, net::UdpClient::StartError>;
		using RunResult = std::expected<void, RunError>;

	private:
		config::ClientConfig config_;

		common::log::DebugOutputLogger debugOutputLogger_;
		common::log::AsyncLogWriter logger_;

		game::ClientWorld world_;
		net::UdpClient udpClient_;
		runtime::ClientRuntime runtime_;
		ui::GameWindow gameWindow_;
		render::GdiRenderer gdiRenderer_;

		std::atomic<bool> isRunning_ = false;

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
	};
}