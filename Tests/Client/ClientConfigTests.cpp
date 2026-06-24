#include "ClientConfigTests.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <Common/Time/TimeTypes.h>

#include <Client/Config/ClientConfigLoader.h>
#include <Client/Config/ClientConfigValidator.h>
#include <Client/Config/ClientTransportType.h>

#include <Tests/TestHelpers.h>

namespace
{
	void RunLoadValidConfigTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath = tests::MakeTempFilePath("WindowsIocpUdp_ClientConfig_DebugTest.ini");

		tests::WriteTextFile(
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
			"LogLevel = Debug\n"
			"AsyncLogWorkerThreadCount = 2\n"
		);

		const client::config::ClientConfigLoadResult loadResult = client::config::ClientConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		tests::Expect(result, loadResult.loadedFromFile, "ClientConfig: valid file loaded");
		tests::Expect(result, loadResult.warningList.empty(), "ClientConfig: valid file has no loader warning");

		const client::config::ClientConfig& config = loadResult.config;

		tests::Expect(result, config.network.serverIp == "192.168.0.10", "ClientConfig: serverIp");
		tests::Expect(result, config.network.serverPort == 9100, "ClientConfig: serverPort");
		tests::Expect(
			result,
			config.network.transportType == client::config::ClientTransportType::Iocp,
			"ClientConfig: transportType"
		);
		tests::Expect(result, config.network.iocpWorkerThreadCount == 2, "ClientConfig: iocpWorkerThreadCount");
		tests::Expect(result, config.network.iocpRecvContextCount == 8, "ClientConfig: iocpRecvContextCount");
		tests::Expect(result, config.timing.updateSleepInterval == common::time::Milliseconds(2), "ClientConfig: updateSleep");
		tests::Expect(result, config.timing.joinRetryInterval == common::time::Milliseconds(1500), "ClientConfig: joinRetry");
		tests::Expect(result, config.timing.roomJoinInterval == common::time::Milliseconds(300), "ClientConfig: roomJoin");
		tests::Expect(result, config.timing.interpolationAdjustStep == common::time::Milliseconds(15), "ClientConfig: adjustStep");
		tests::Expect(result, config.interpolation.defaultDelay == common::time::Milliseconds(120), "ClientConfig: defaultDelay");
		tests::Expect(result, config.interpolation.minDelay == common::time::Milliseconds(10), "ClientConfig: minDelay");
		tests::Expect(result, config.interpolation.maxDelay == common::time::Milliseconds(600), "ClientConfig: maxDelay");
		tests::Expect(result, config.snapshot.assemblyTimeout == common::time::Milliseconds(700), "ClientConfig: assemblyTimeout");
		tests::Expect(result, config.simulation.tickInterval == common::time::Milliseconds(40), "ClientConfig: simulation tick");
		tests::Expect(result, config.simulation.deltaSeconds == 0.04F, "ClientConfig: simulation delta");
		tests::Expect(result, !config.diagnostics.enableChunkAssemblerDebugTests, "ClientConfig: debug test flag");
		tests::Expect(
			result,
			loadResult.config.diagnostics.logLevel == common::log::LogLevel::Debug,
			"ClientConfig: load diagnostics log level"
		);

		tests::Expect(
			result,
			loadResult.config.diagnostics.asyncLogWorkerThreadCount == 2,
			"ClientConfig: load async log worker thread count"
		);
	}

	void RunLoadInvalidConfigTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath = tests::MakeTempFilePath("WindowsIocpUdp_ClientConfig_DebugTest.ini");

		tests::WriteTextFile(
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
			"LogLevel = Verbose\n"
			"AsyncLogWorkerThreadCount = 0\n"
		);

		const client::config::ClientConfigLoadResult loadResult = client::config::ClientConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		tests::Expect(result, loadResult.loadedFromFile, "ClientConfig: invalid file loaded");
		tests::Expect(result, loadResult.warningList.size() >= 9, "ClientConfig: invalid file warning count");
		tests::Expect(
			result,
			loadResult.config.diagnostics.logLevel == client::config::defaultLogLevel,
			"ClientConfig: invalid log level keeps default"
		);

		tests::Expect(
			result,
			loadResult.config.diagnostics.asyncLogWorkerThreadCount == client::config::defaultAsyncLogWorkerThreadCount,
			"ClientConfig: invalid async log worker count keeps default"
		);
	}

	void RunMissingFileTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath = tests::MakeTempFilePath("WindowsIocpUdp_ClientConfig_DebugTest.ini");
		std::filesystem::remove(filePath);

		const client::config::ClientConfigLoadResult loadResult = client::config::ClientConfigLoader::Load(filePath);

		tests::Expect(result, !loadResult.loadedFromFile, "ClientConfig: missing file not loaded");
		tests::Expect(result, !loadResult.warningList.empty(), "ClientConfig: missing file warning");
	}

	void RunValidatorNormalizeTest(tests::DebugTestResult& result)
	{
		client::config::ClientConfig config{};
		const client::config::ClientConfig defaultConfig{};

		config.network.serverIp.clear();
		config.network.serverPort = 0;
		config.network.iocpWorkerThreadCount = 0;
		config.network.iocpRecvContextCount = 0;
		config.timing.updateSleepInterval = common::time::Milliseconds(0);
		config.timing.joinRetryInterval = common::time::Milliseconds(0);
		config.timing.roomJoinInterval = common::time::Milliseconds(0);
		config.timing.interpolationAdjustStep = common::time::Milliseconds(0);
		config.interpolation.defaultDelay = common::time::Milliseconds(999);
		config.interpolation.minDelay = common::time::Milliseconds(600);
		config.interpolation.maxDelay = common::time::Milliseconds(100);
		config.snapshot.assemblyTimeout = common::time::Milliseconds(0);
		config.simulation.tickInterval = common::time::Milliseconds(0);
		config.simulation.deltaSeconds = 0.0F;

		const std::vector<client::config::ClientConfigWarning> warningList = client::config::ClientConfigValidator::ValidateAndNormalize(config);

		tests::Expect(result, !warningList.empty(), "ClientConfigValidator: warning generated");
		tests::Expect(result, config.network.serverIp == defaultConfig.network.serverIp, "ClientConfigValidator: serverIp normalized");
		tests::Expect(result, config.network.serverPort == defaultConfig.network.serverPort, "ClientConfigValidator: serverPort normalized");
		tests::Expect(
			result,
			config.network.iocpWorkerThreadCount == defaultConfig.network.iocpWorkerThreadCount,
			"ClientConfigValidator: iocp worker thread count normalized"
		);
		tests::Expect(
			result,
			config.network.iocpRecvContextCount == defaultConfig.network.iocpRecvContextCount,
			"ClientConfigValidator: iocp recv context count normalized"
		);
		tests::Expect(result, config.timing.updateSleepInterval == defaultConfig.timing.updateSleepInterval,
			"ClientConfigValidator: update sleep normalized");
		tests::Expect(result, config.interpolation.minDelay == defaultConfig.interpolation.minDelay,
			"ClientConfigValidator: min delay normalized");
		tests::Expect(result, config.interpolation.maxDelay == defaultConfig.interpolation.maxDelay,
			"ClientConfigValidator: max delay normalized");
		tests::Expect(result, config.snapshot.assemblyTimeout == defaultConfig.snapshot.assemblyTimeout,
			"ClientConfigValidator: snapshot timeout normalized");
		tests::Expect(result, config.simulation.tickInterval == defaultConfig.simulation.tickInterval,
			"ClientConfigValidator: simulation tick normalized");
		tests::Expect(result, config.simulation.deltaSeconds == defaultConfig.simulation.deltaSeconds,
			"ClientConfigValidator: simulation delta normalized");

		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Network.ServerIp cannot be empty. Default server ip will be used."),
			"ClientConfigValidator: server ip warning message"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Network.ServerPort cannot be 0. Default server port will be used."),
			"ClientConfigValidator: server port warning message"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Network.IocpWorkerThreadCount must be greater than 0. Default IOCP worker thread count will be used."),
			"ClientConfigValidator: iocp worker warning message"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Network.IocpRecvContextCount must be greater than 0. Default IOCP recv context count will be used."),
			"ClientConfigValidator: iocp recv warning message"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Interpolation.MinDelayMs cannot be greater than Interpolation.MaxDelayMs. Default interpolation range will be used."),
			"ClientConfigValidator: interpolation range warning message"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Simulation.TickIntervalMs must be greater than 0. Default tick interval will be used."),
			"ClientConfigValidator: simulation tick warning message"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Simulation.DeltaSeconds must be greater than 0. Default delta seconds will be used."),
			"ClientConfigValidator: simulation delta warning message"
		);
	}

	void RunLoadTransportTypeCaseInsensitiveTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath = tests::MakeTempFilePath("WindowsIocpUdp_ClientConfig_DebugTest.ini");

		tests::WriteTextFile(
			filePath,
			"[Network]\n"
			"TransportType=IOCP\n"
			"IocpWorkerThreadCount=3\n"
			"IocpRecvContextCount=6\n"
		);

		const client::config::ClientConfigLoadResult loadResult = client::config::ClientConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		tests::Expect(result, loadResult.loadedFromFile, "ClientConfig: transport type case file loaded");
		tests::Expect(result, loadResult.warningList.empty(), "ClientConfig: transport type case has no loader warning");
		tests::Expect(
			result,
			loadResult.config.network.transportType == client::config::ClientTransportType::Iocp,
			"ClientConfig: transport type case insensitive"
		);
		tests::Expect(
			result,
			loadResult.config.network.iocpWorkerThreadCount == 3,
			"ClientConfig: transport type case worker count"
		);
		tests::Expect(
			result,
			loadResult.config.network.iocpRecvContextCount == 6,
			"ClientConfig: transport type case recv context count"
		);
	}

	void RunValidatorInterpolationDefaultDelayClampTest(tests::DebugTestResult& result)
	{
		client::config::ClientConfig lowerConfig{};
		lowerConfig.interpolation.minDelay = common::time::Milliseconds(100);
		lowerConfig.interpolation.maxDelay = common::time::Milliseconds(300);
		lowerConfig.interpolation.defaultDelay = common::time::Milliseconds(50);

		const std::vector<client::config::ClientConfigWarning> lowerWarningList =
			client::config::ClientConfigValidator::ValidateAndNormalize(lowerConfig);

		tests::Expect(
			result,
			lowerConfig.interpolation.defaultDelay == common::time::Milliseconds(100),
			"ClientConfigValidator: default delay clamped to min"
		);

		tests::Expect(
			result,
			tests::ContainsWarningMessage(lowerWarningList, "Interpolation.DefaultDelayMs is lower than MinDelayMs. It will be clamped to MinDelayMs."),
			"ClientConfigValidator: default delay lower warning message"
		);

		client::config::ClientConfig upperConfig{};
		upperConfig.interpolation.minDelay = common::time::Milliseconds(100);
		upperConfig.interpolation.maxDelay = common::time::Milliseconds(300);
		upperConfig.interpolation.defaultDelay = common::time::Milliseconds(500);

		const std::vector<client::config::ClientConfigWarning> upperWarningList =
			client::config::ClientConfigValidator::ValidateAndNormalize(upperConfig);

		tests::Expect(
			result,
			upperConfig.interpolation.defaultDelay == common::time::Milliseconds(300),
			"ClientConfigValidator: default delay clamped to max"
		);

		tests::Expect(
			result,
			tests::ContainsWarningMessage(upperWarningList, "Interpolation.DefaultDelayMs is greater than MaxDelayMs. It will be clamped to MaxDelayMs."),
			"ClientConfigValidator: default delay upper warning message"
		);
	}

	void RunLoadValidatedNormalizesConfigTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath =
			tests::MakeTempFilePath("WindowsIocpUdp_ClientConfig_LoadValidated_DebugTest.ini");

		tests::WriteTextFile(
			filePath,
			"[Network]\n"
			"ServerIp=\n"
			"IocpWorkerThreadCount=0\n"
			"IocpRecvContextCount=0\n"
			"\n"
			"[Timing]\n"
			"UpdateSleepMs=0\n"
		);

		client::config::ClientConfigLoadResult loadResult =
			client::config::ClientConfigLoader::LoadValidated(filePath);

		std::filesystem::remove(filePath);

		const client::config::ClientConfig defaultConfig{};

		tests::Expect(
			result,
			loadResult.loadedFromFile,
			"ClientConfig: LoadValidated file loaded"
		);
		tests::Expect(
			result,
			loadResult.config.network.serverIp == defaultConfig.network.serverIp,
			"ClientConfig: LoadValidated normalizes server ip"
		);
		tests::Expect(
			result,
			loadResult.config.network.iocpWorkerThreadCount == defaultConfig.network.iocpWorkerThreadCount,
			"ClientConfig: LoadValidated normalizes IOCP worker thread count"
		);
		tests::Expect(
			result,
			loadResult.config.timing.updateSleepInterval == defaultConfig.timing.updateSleepInterval,
			"ClientConfig: LoadValidated normalizes update sleep"
		);
		tests::Expect(
			result,
			!loadResult.warningList.empty(),
			"ClientConfig: LoadValidated returns validation warnings"
		);
	}
}

namespace tests::client
{
	tests::DebugTestResult RunClientConfigTests()
	{
		tests::DebugTestResult result{};

		RunLoadValidConfigTest(result);
		RunLoadInvalidConfigTest(result);
		RunLoadTransportTypeCaseInsensitiveTest(result);
		RunMissingFileTest(result);
		RunValidatorNormalizeTest(result);
		RunValidatorInterpolationDefaultDelayClampTest(result);
		RunLoadValidatedNormalizesConfigTest(result);

		return result;
	}
}