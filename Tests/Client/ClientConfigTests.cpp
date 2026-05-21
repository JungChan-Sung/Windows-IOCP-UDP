#include "ClientConfigTests.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <Client/Config/ClientConfigLoader.h>
#include <Client/Config/ClientConfigValidator.h>
#include <Client/Config/ClientTransportType.h>

namespace
{
	[[nodiscard]] std::filesystem::path MakeTempClientConfigPath()
	{
		return std::filesystem::temp_directory_path() / "WindowsIocpUdp_ClientConfig_DebugTest.ini";
	}

	void WriteTextFile(const std::filesystem::path& filePath, const std::string& text)
	{
		std::ofstream file(filePath, std::ios::trunc);
		file << text;
	}

	[[nodiscard]] bool ContainsWarningMessage(
		const std::vector<client::config::ClientConfigWarning>& warningList,
		std::string_view message
	)
	{
		return std::ranges::any_of(
			warningList,
			[message](const client::config::ClientConfigWarning& warning)
			{
				return warning.message == message;
			}
		);
	}

	void RunLoadValidConfigTest(common::diagnostics::DebugTestResult& result)
	{
		const std::filesystem::path filePath = MakeTempClientConfigPath();

		WriteTextFile(
			filePath,
			"[Network]\n"
			"ServerIp=192.168.0.10\n"
			"ServerPort=9100\n"
			"TransportType=Iocp\n"
			"IocpWorkerThreadCount=2\n"
			"IocpRecvContextCount=8\n"
			"\n"
			"[Timing]\n"
			"UpdateSleepMs=2\n"
			"JoinRetryMs=1500\n"
			"RoomJoinMs=300\n"
			"InterpolationAdjustStepMs=15\n"
			"\n"
			"[Interpolation]\n"
			"DefaultDelayMs=120\n"
			"MinDelayMs=10\n"
			"MaxDelayMs=600\n"
			"\n"
			"[Snapshot]\n"
			"AssemblyTimeoutMs=700\n"
			"\n"
			"[Simulation]\n"
			"TickIntervalMs=40\n"
			"DeltaSeconds=0.04\n"
			"\n"
			"[Diagnostics]\n"
			"EnableChunkAssemblerDebugTests=false\n"
		);

		const client::config::ClientConfigLoadResult loadResult = client::config::ClientConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		common::diagnostics::Expect(result, loadResult.loadedFromFile, "ClientConfig: valid file loaded");
		common::diagnostics::Expect(result, loadResult.warningList.empty(), "ClientConfig: valid file has no loader warning");

		const client::config::ClientConfig& config = loadResult.config;

		common::diagnostics::Expect(result, config.network.serverIp == "192.168.0.10", "ClientConfig: serverIp");
		common::diagnostics::Expect(result, config.network.serverPort == 9100, "ClientConfig: serverPort");
		common::diagnostics::Expect(
			result,
			config.network.transportType == client::config::ClientTransportType::Iocp,
			"ClientConfig: transportType"
		);
		common::diagnostics::Expect(result, config.network.iocpWorkerThreadCount == 2, "ClientConfig: iocpWorkerThreadCount");
		common::diagnostics::Expect(result, config.network.iocpRecvContextCount == 8, "ClientConfig: iocpRecvContextCount");
		common::diagnostics::Expect(result, config.timing.updateSleepInterval == std::chrono::milliseconds(2), "ClientConfig: updateSleep");
		common::diagnostics::Expect(result, config.timing.joinRetryInterval == std::chrono::milliseconds(1500), "ClientConfig: joinRetry");
		common::diagnostics::Expect(result, config.timing.roomJoinInterval == std::chrono::milliseconds(300), "ClientConfig: roomJoin");
		common::diagnostics::Expect(result, config.timing.interpolationAdjustStep == std::chrono::milliseconds(15), "ClientConfig: adjustStep");
		common::diagnostics::Expect(result, config.interpolation.defaultDelay == std::chrono::milliseconds(120), "ClientConfig: defaultDelay");
		common::diagnostics::Expect(result, config.interpolation.minDelay == std::chrono::milliseconds(10), "ClientConfig: minDelay");
		common::diagnostics::Expect(result, config.interpolation.maxDelay == std::chrono::milliseconds(600), "ClientConfig: maxDelay");
		common::diagnostics::Expect(result, config.snapshot.assemblyTimeout == std::chrono::milliseconds(700), "ClientConfig: assemblyTimeout");
		common::diagnostics::Expect(result, config.simulation.tickInterval == std::chrono::milliseconds(40), "ClientConfig: simulation tick");
		common::diagnostics::Expect(result, config.simulation.deltaSeconds == 0.04F, "ClientConfig: simulation delta");
		common::diagnostics::Expect(result, !config.diagnostics.enableChunkAssemblerDebugTests, "ClientConfig: debug test flag");
	}

	void RunLoadInvalidConfigTest(common::diagnostics::DebugTestResult& result)
	{
		const std::filesystem::path filePath = MakeTempClientConfigPath();

		WriteTextFile(
			filePath,
			"ServerPort=9000\n"
			"\n"
			"[Network]\n"
			"ServerIp=\n"
			"ServerPort=999999\n"
			"TransportType=InvalidTransport\n"
			"IocpWorkerThreadCount=0\n"
			"IocpRecvContextCount=0\n"
			"UnknownKey=1\n"
			"\n"
			"[Unknown]\n"
			"Value=1\n"
			"\n"
			"[Timing]\n"
			"UpdateSleepMs=0\n"
			"\n"
			"[Diagnostics]\n"
			"EnableChunkAssemblerDebugTests=maybe\n"
		);

		const client::config::ClientConfigLoadResult loadResult = client::config::ClientConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		common::diagnostics::Expect(result, loadResult.loadedFromFile, "ClientConfig: invalid file loaded");
		common::diagnostics::Expect(result, loadResult.warningList.size() >= 9, "ClientConfig: invalid file warning count");
	}

	void RunMissingFileTest(common::diagnostics::DebugTestResult& result)
	{
		const std::filesystem::path filePath = MakeTempClientConfigPath();
		std::filesystem::remove(filePath);

		const client::config::ClientConfigLoadResult loadResult = client::config::ClientConfigLoader::Load(filePath);

		common::diagnostics::Expect(result, !loadResult.loadedFromFile, "ClientConfig: missing file not loaded");
		common::diagnostics::Expect(result, !loadResult.warningList.empty(), "ClientConfig: missing file warning");
	}

	void RunValidatorNormalizeTest(common::diagnostics::DebugTestResult& result)
	{
		client::config::ClientConfig config{};
		const client::config::ClientConfig defaultConfig{};

		config.network.serverIp.clear();
		config.network.serverPort = 0;
		config.network.iocpWorkerThreadCount = 0;
		config.network.iocpRecvContextCount = 0;
		config.timing.updateSleepInterval = std::chrono::milliseconds(0);
		config.timing.joinRetryInterval = std::chrono::milliseconds(0);
		config.timing.roomJoinInterval = std::chrono::milliseconds(0);
		config.timing.interpolationAdjustStep = std::chrono::milliseconds(0);
		config.interpolation.defaultDelay = std::chrono::milliseconds(999);
		config.interpolation.minDelay = std::chrono::milliseconds(600);
		config.interpolation.maxDelay = std::chrono::milliseconds(100);
		config.snapshot.assemblyTimeout = std::chrono::milliseconds(0);
		config.simulation.tickInterval = std::chrono::milliseconds(0);
		config.simulation.deltaSeconds = 0.0F;

		const std::vector<client::config::ClientConfigWarning> warningList = client::config::ClientConfigValidator::ValidateAndNormalize(config);

		common::diagnostics::Expect(result, !warningList.empty(), "ClientConfigValidator: warning generated");
		common::diagnostics::Expect(result, config.network.serverIp == defaultConfig.network.serverIp, "ClientConfigValidator: serverIp normalized");
		common::diagnostics::Expect(result, config.network.serverPort == defaultConfig.network.serverPort, "ClientConfigValidator: serverPort normalized");
		common::diagnostics::Expect(
			result,
			config.network.iocpWorkerThreadCount == defaultConfig.network.iocpWorkerThreadCount,
			"ClientConfigValidator: iocp worker thread count normalized"
		);
		common::diagnostics::Expect(
			result,
			config.network.iocpRecvContextCount == defaultConfig.network.iocpRecvContextCount,
			"ClientConfigValidator: iocp recv context count normalized"
		);
		common::diagnostics::Expect(result, config.timing.updateSleepInterval == defaultConfig.timing.updateSleepInterval,
			"ClientConfigValidator: update sleep normalized");
		common::diagnostics::Expect(result, config.interpolation.minDelay == defaultConfig.interpolation.minDelay,
			"ClientConfigValidator: min delay normalized");
		common::diagnostics::Expect(result, config.interpolation.maxDelay == defaultConfig.interpolation.maxDelay,
			"ClientConfigValidator: max delay normalized");
		common::diagnostics::Expect(result, config.snapshot.assemblyTimeout == defaultConfig.snapshot.assemblyTimeout,
			"ClientConfigValidator: snapshot timeout normalized");
		common::diagnostics::Expect(result, config.simulation.tickInterval == defaultConfig.simulation.tickInterval,
			"ClientConfigValidator: simulation tick normalized");
		common::diagnostics::Expect(result, config.simulation.deltaSeconds == defaultConfig.simulation.deltaSeconds,
			"ClientConfigValidator: simulation delta normalized");

		common::diagnostics::Expect(
			result,
			ContainsWarningMessage(warningList, "Network.ServerIp cannot be empty. Default server ip will be used."),
			"ClientConfigValidator: server ip warning message"
		);
		common::diagnostics::Expect(
			result,
			ContainsWarningMessage(warningList, "Network.ServerPort cannot be 0. Default server port will be used."),
			"ClientConfigValidator: server port warning message"
		);
		common::diagnostics::Expect(
			result,
			ContainsWarningMessage(warningList, "Network.IocpWorkerThreadCount must be greater than 0. Default IOCP worker thread count will be used."),
			"ClientConfigValidator: iocp worker warning message"
		);
		common::diagnostics::Expect(
			result,
			ContainsWarningMessage(warningList, "Network.IocpRecvContextCount must be greater than 0. Default IOCP recv context count will be used."),
			"ClientConfigValidator: iocp recv warning message"
		);
		common::diagnostics::Expect(
			result,
			ContainsWarningMessage(warningList, "Interpolation.MinDelayMs cannot be greater than Interpolation.MaxDelayMs. Default interpolation range will be used."),
			"ClientConfigValidator: interpolation range warning message"
		);
		common::diagnostics::Expect(
			result,
			ContainsWarningMessage(warningList, "Simulation.TickIntervalMs must be greater than 0. Default tick interval will be used."),
			"ClientConfigValidator: simulation tick warning message"
		);
		common::diagnostics::Expect(
			result,
			ContainsWarningMessage(warningList, "Simulation.DeltaSeconds must be greater than 0. Default delta seconds will be used."),
			"ClientConfigValidator: simulation delta warning message"
		);
	}

	void RunLoadTransportTypeCaseInsensitiveTest(common::diagnostics::DebugTestResult& result)
	{
		const std::filesystem::path filePath = MakeTempClientConfigPath();

		WriteTextFile(
			filePath,
			"[Network]\n"
			"TransportType=IOCP\n"
			"IocpWorkerThreadCount=3\n"
			"IocpRecvContextCount=6\n"
		);

		const client::config::ClientConfigLoadResult loadResult = client::config::ClientConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		common::diagnostics::Expect(result, loadResult.loadedFromFile, "ClientConfig: transport type case file loaded");
		common::diagnostics::Expect(result, loadResult.warningList.empty(), "ClientConfig: transport type case has no loader warning");
		common::diagnostics::Expect(
			result,
			loadResult.config.network.transportType == client::config::ClientTransportType::Iocp,
			"ClientConfig: transport type case insensitive"
		);
		common::diagnostics::Expect(
			result,
			loadResult.config.network.iocpWorkerThreadCount == 3,
			"ClientConfig: transport type case worker count"
		);
		common::diagnostics::Expect(
			result,
			loadResult.config.network.iocpRecvContextCount == 6,
			"ClientConfig: transport type case recv context count"
		);
	}

	void RunValidatorInterpolationDefaultDelayClampTest(common::diagnostics::DebugTestResult& result)
	{
		client::config::ClientConfig lowerConfig{};
		lowerConfig.interpolation.minDelay = std::chrono::milliseconds(100);
		lowerConfig.interpolation.maxDelay = std::chrono::milliseconds(300);
		lowerConfig.interpolation.defaultDelay = std::chrono::milliseconds(50);

		const std::vector<client::config::ClientConfigWarning> lowerWarningList =
			client::config::ClientConfigValidator::ValidateAndNormalize(lowerConfig);

		common::diagnostics::Expect(
			result,
			lowerConfig.interpolation.defaultDelay == std::chrono::milliseconds(100),
			"ClientConfigValidator: default delay clamped to min"
		);

		common::diagnostics::Expect(
			result,
			ContainsWarningMessage(lowerWarningList, "Interpolation.DefaultDelayMs is lower than MinDelayMs. It will be clamped to MinDelayMs."),
			"ClientConfigValidator: default delay lower warning message"
		);

		client::config::ClientConfig upperConfig{};
		upperConfig.interpolation.minDelay = std::chrono::milliseconds(100);
		upperConfig.interpolation.maxDelay = std::chrono::milliseconds(300);
		upperConfig.interpolation.defaultDelay = std::chrono::milliseconds(500);

		const std::vector<client::config::ClientConfigWarning> upperWarningList =
			client::config::ClientConfigValidator::ValidateAndNormalize(upperConfig);

		common::diagnostics::Expect(
			result,
			upperConfig.interpolation.defaultDelay == std::chrono::milliseconds(300),
			"ClientConfigValidator: default delay clamped to max"
		);

		common::diagnostics::Expect(
			result,
			ContainsWarningMessage(upperWarningList, "Interpolation.DefaultDelayMs is greater than MaxDelayMs. It will be clamped to MaxDelayMs."),
			"ClientConfigValidator: default delay upper warning message"
		);
	}
}

namespace tests::client
{
	common::diagnostics::DebugTestResult RunClientConfigTests()
	{
		common::diagnostics::DebugTestResult result{};

		RunLoadValidConfigTest(result);
		RunLoadInvalidConfigTest(result);
		RunLoadTransportTypeCaseInsensitiveTest(result);
		RunMissingFileTest(result);
		RunValidatorNormalizeTest(result);
		RunValidatorInterpolationDefaultDelayClampTest(result);

		return result;
	}
}