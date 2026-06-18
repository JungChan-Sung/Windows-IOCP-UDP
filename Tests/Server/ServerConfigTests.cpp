#include "ServerConfigTests.h"

#include <chrono>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include <Common/Log/LogLevel.h>

#include <Server/Config/ServerConfigLoader.h>
#include <Server/Config/ServerConfigValidator.h>

#include <Tests/TestHelpers.h>

namespace
{
	void RunLoadValidConfigTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath = tests::MakeTempFilePath("WindowsIocpUdp_ServerConfig_DebugTest.ini");

		tests::WriteTextFile(
			filePath,
			"[Network]\n"
			"Port=9100\n"
			"WorkerThreadCount=2\n"
			"RecvContextCount=64\n"
			"\n"
			"[Session]\n"
			"InitialRoomId=3\n"
			"PeerTimeoutSeconds=15\n"
			"\n"
			"[ReliableUdp]\n"
			"MaxPendingPacketCount=128\n"
			"MaxResendCount=7\n"
			"ResendIntervalMilliseconds=250\n"
			"\n"
			"[Tick]\n"
			"TickIntervalMs=33\n"
			"FixedDeltaSeconds=0.033\n"
			"\n"
			"[GameRule]\n"
			"InitialPlayerHp=5\n"
			"RespawnDelaySeconds=2.5\n"
			"RespawnInvincibilitySeconds=1.5\n"
			"HitFlashDurationSeconds=0.25\n"
			"\n"
			"[Weapon.Basic]\n"
			"BulletDamage=2\n"
			"BulletSpeed=700.0\n"
			"BulletLifeSeconds=2.0\n"
			"BulletRadius=7.5\n"
			"FireCooldownSeconds=0.2\n"
			"\n"
			"[Diagnostics]\n"
			"EnableStatusLog=false\n"
			"StatusLogIntervalSeconds=20\n"
			"LogLevel=Debug\n"
			"AsyncLogWorkerThreadCount=2\n"
		);

		const server::config::ServerConfigLoadResult loadResult = server::config::ServerConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		tests::Expect(result, loadResult.loadedFromFile, "ServerConfig: valid file loaded");
		tests::Expect(result, loadResult.warningList.empty(), "ServerConfig: valid file has no loader warning");

		const server::config::ServerConfig& config = loadResult.config;

		tests::Expect(result, config.network.port == 9100, "ServerConfig: port");
		tests::Expect(result, config.network.workerThreadCount == 2, "ServerConfig: workerThreadCount");
		tests::Expect(result, config.network.recvContextCount == 64, "ServerConfig: recvContextCount");
		tests::Expect(result, config.session.initialRoomId == 3, "ServerConfig: initialRoomId");
		tests::Expect(result, config.session.peerTimeout == std::chrono::seconds(15), "ServerConfig: peerTimeout");
		tests::Expect(result, config.reliableUdp.maxPendingPacketCount == 128, "ServerConfig: reliable maxPendingPacketCount");
		tests::Expect(result, config.reliableUdp.maxResendCount == 7, "ServerConfig: reliable maxResendCount");
		tests::Expect(result, config.reliableUdp.resendInterval == common::time::Milliseconds(250), "ServerConfig: reliable resendIntervalMilliseconds");
		tests::Expect(result, config.tick.tickInterval == common::time::Milliseconds(33), "ServerConfig: tickInterval");
		tests::Expect(result, config.tick.fixedDeltaSeconds == 0.033F, "ServerConfig: fixedDeltaSeconds");
		tests::Expect(result, config.gameRule.initialPlayerHp == 5, "ServerConfig: initialPlayerHp");
		tests::Expect(result, config.gameRule.respawnDelaySeconds == 2.5F, "ServerConfig: respawnDelaySeconds");
		tests::Expect(result, config.gameRule.respawnInvincibilitySeconds == 1.5F, "ServerConfig: respawnInvincibilitySeconds");
		tests::Expect(result, config.gameRule.hitFlashDurationSeconds == 0.25F, "ServerConfig: hitFlashDurationSeconds");
		tests::Expect(result, config.weaponRule.basicWeaponRule.bulletDamage == 2, "ServerConfig: bulletDamage");
		tests::Expect(result, config.weaponRule.basicWeaponRule.bulletSpeed == 700.0F, "ServerConfig: bulletSpeed");
		tests::Expect(result, config.weaponRule.basicWeaponRule.bulletLifeSeconds == 2.0F, "ServerConfig: bulletLifeSeconds");
		tests::Expect(result, config.weaponRule.basicWeaponRule.bulletRadius == 7.5F, "ServerConfig: bulletRadius");
		tests::Expect(result, config.weaponRule.basicWeaponRule.fireCooldownSeconds == 0.2F, "ServerConfig: fireCooldownSeconds");
		tests::Expect(result, !config.diagnostics.enableStatusLog, "ServerConfig: enableStatusLog");
		tests::Expect(result, config.diagnostics.statusLogInterval == std::chrono::seconds(20), "ServerConfig: statusLogInterval");
		tests::Expect(result, config.diagnostics.logLevel == common::log::LogLevel::Debug, "ServerConfig: logLevel");
		tests::Expect(result, config.diagnostics.asyncLogWorkerThreadCount == 2, "ServerConfig: asyncLogWorkerThreadCount");
	}

	void RunLoadInvalidConfigTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath = tests::MakeTempFilePath("WindowsIocpUdp_ServerConfig_DebugTest.ini");

		tests::WriteTextFile(
			filePath,
			"Port=9000\n"
			"\n"
			"[Network]\n"
			"Port=abc\n"
			"UnknownKey=1\n"
			"\n"
			"[Unknown]\n"
			"Value=1\n"
			"\n"
			"[ReliableUdp]\n"
			"MaxPendingPacketCount=0\n"
			"MaxResendCount=-1\n"
			"ResendIntervalMilliseconds=0\n"
			"\n"
			"[Tick]\n"
			"TickIntervalMs=0\n"
			"FixedDeltaSeconds=bad\n"
			"[Diagnostics]\n"
			"LogLevel=Verbose\n"
			"AsyncLogWorkerThreadCount=0\n"
		);

		const server::config::ServerConfigLoadResult loadResult = server::config::ServerConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		tests::Expect(result, loadResult.loadedFromFile, "ServerConfig: invalid file loaded");
		tests::Expect(result, loadResult.warningList.size() >= 10, "ServerConfig: invalid file warning count");
	}

	void RunMissingFileTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath = tests::MakeTempFilePath("WindowsIocpUdp_ServerConfig_DebugTest.ini");
		std::filesystem::remove(filePath);

		const server::config::ServerConfigLoadResult loadResult = server::config::ServerConfigLoader::Load(filePath);

		tests::Expect(result, !loadResult.loadedFromFile, "ServerConfig: missing file not loaded");
		tests::Expect(result, !loadResult.warningList.empty(), "ServerConfig: missing file warning");
	}

	void RunValidatorNormalizeTest(tests::DebugTestResult& result)
	{
		server::config::ServerConfig config{};
		const server::config::ServerConfig defaultConfig{};

		config.network.port = 0;
		config.session.initialRoomId = 0;
		config.session.peerTimeout = std::chrono::seconds(0);
		config.reliableUdp.maxPendingPacketCount = 0;
		config.reliableUdp.maxResendCount = -1;
		config.reliableUdp.resendInterval = common::time::Milliseconds(0);
		config.tick.tickInterval = common::time::Milliseconds(0);
		config.tick.fixedDeltaSeconds = 0.0F;
		config.gameRule.initialPlayerHp = 0;
		config.gameRule.respawnDelaySeconds = -1.0F;
		config.gameRule.respawnInvincibilitySeconds = -1.0F;
		config.gameRule.hitFlashDurationSeconds = -1.0F;
		config.weaponRule.basicWeaponRule.bulletDamage = 0;
		config.weaponRule.basicWeaponRule.bulletSpeed = 0.0F;
		config.weaponRule.basicWeaponRule.bulletLifeSeconds = 0.0F;
		config.weaponRule.basicWeaponRule.bulletRadius = 0.0F;
		config.weaponRule.basicWeaponRule.fireCooldownSeconds = -1.0F;
		config.diagnostics.statusLogInterval = std::chrono::seconds(0);
		config.diagnostics.asyncLogWorkerThreadCount = 0;

		const std::vector<server::config::ServerConfigWarning> warningList = server::config::ServerConfigValidator::ValidateAndNormalize(config);

		tests::Expect(result, !warningList.empty(), "ServerConfigValidator: warning generated");
		tests::Expect(result, config.network.port == defaultConfig.network.port, "ServerConfigValidator: port normalized");
		tests::Expect(result, config.network.workerThreadCount > 0, "ServerConfigValidator: worker count resolved");
		tests::Expect(result, config.network.recvContextCount > 0, "ServerConfigValidator: recv context count resolved");
		tests::Expect(result, config.session.initialRoomId == defaultConfig.session.initialRoomId, "ServerConfigValidator: room normalized");
		tests::Expect(result, config.session.peerTimeout == defaultConfig.session.peerTimeout, "ServerConfigValidator: timeout normalized");
		tests::Expect(
			result,
			config.reliableUdp.maxPendingPacketCount == defaultConfig.reliableUdp.maxPendingPacketCount,
			"ServerConfigValidator: reliable max pending packet count normalized"
		);
		tests::Expect(
			result,
			config.reliableUdp.maxResendCount == defaultConfig.reliableUdp.maxResendCount,
			"ServerConfigValidator: reliable max resend count normalized"
		);
		tests::Expect(
			result,
			config.reliableUdp.resendInterval == defaultConfig.reliableUdp.resendInterval,
			"ServerConfigValidator: reliable resend interval normalized"
		);
		tests::Expect(result, config.tick.tickInterval == defaultConfig.tick.tickInterval, "ServerConfigValidator: tick interval normalized");
		tests::Expect(result, config.tick.fixedDeltaSeconds == defaultConfig.tick.fixedDeltaSeconds, "ServerConfigValidator: delta normalized");
		tests::Expect(result, config.gameRule.initialPlayerHp == defaultConfig.gameRule.initialPlayerHp, "ServerConfigValidator: hp normalized");
		tests::Expect(result, config.weaponRule.basicWeaponRule.bulletDamage == defaultConfig.weaponRule.basicWeaponRule.bulletDamage,
			"ServerConfigValidator: damage normalized"
		);
		tests::Expect(result, config.diagnostics.statusLogInterval == defaultConfig.diagnostics.statusLogInterval,
			"ServerConfigValidator: status interval normalized"
		);

		tests::Expect(
			result,
			config.diagnostics.asyncLogWorkerThreadCount == defaultConfig.diagnostics.asyncLogWorkerThreadCount,
			"ServerConfigValidator: async log worker count normalized"
		);

		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Network.Port cannot be 0. Default port will be used."),
			"ServerConfigValidator: port warning message"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(
				warningList,
				"ReliableUdp.MaxPendingPacketCount must be greater than 0. Default max pending packet count will be used."
			),
			"ServerConfigValidator: reliable max pending packet count warning message"
		);

		tests::Expect(
			result,
			tests::ContainsWarningMessage(
				warningList,
				"ReliableUdp.MaxResendCount must be greater than or equal to 0. Default max resend count will be used."
			),
			"ServerConfigValidator: reliable max resend count warning message"
		);

		tests::Expect(
			result,
			tests::ContainsWarningMessage(
				warningList,
				"ReliableUdp.ResendIntervalMs must be greater than 0. Default resend interval will be used."
			),
			"ServerConfigValidator: reliable resend interval warning message"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Tick.TickIntervalMs must be greater than 0. Default tick interval will be used."),
			"ServerConfigValidator: tick interval warning message"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Tick.FixedDeltaSeconds must be greater than 0. Default delta will be used."),
			"ServerConfigValidator: fixed delta warning message"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(warningList, "Diagnostics.AsyncLogWorkerThreadCount cannot be 0. Default value will be used."),
			"ServerConfigValidator: async log worker warning message"
		);
	}

	void RunLoadLogLevelAliasTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath = tests::MakeTempFilePath("WindowsIocpUdp_ServerConfig_DebugTest.ini");

		tests::WriteTextFile(
			filePath,
			"[Diagnostics]\n"
			"LogLevel=warn\n"
			"AsyncLogWorkerThreadCount=3\n"
		);

		const server::config::ServerConfigLoadResult loadResult = server::config::ServerConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		tests::Expect(result, loadResult.loadedFromFile, "ServerConfig: log level alias file loaded");
		tests::Expect(result, loadResult.warningList.empty(), "ServerConfig: log level alias has no loader warning");
		tests::Expect(
			result,
			loadResult.config.diagnostics.logLevel == common::log::LogLevel::Warning,
			"ServerConfig: log level warn alias"
		);
		tests::Expect(
			result,
			loadResult.config.diagnostics.asyncLogWorkerThreadCount == 3,
			"ServerConfig: async log worker count alias test"
		);
	}

	void RunValidatorTickDeltaMismatchWarningTest(tests::DebugTestResult& result)
	{
		server::config::ServerConfig config{};

		config.tick.tickInterval = common::time::Milliseconds(50);
		config.tick.fixedDeltaSeconds = 0.033F;

		const std::vector<server::config::ServerConfigWarning> warningList =
			server::config::ServerConfigValidator::ValidateAndNormalize(config);

		const bool hasMismatchWarning = std::ranges::any_of(
			warningList,
			[](const server::config::ServerConfigWarning& warning)
			{
				return warning.message.find("Tick.FixedDeltaSeconds does not match Tick.TickIntervalMs.") != std::string::npos;
			}
		);

		tests::Expect(result, hasMismatchWarning, "ServerConfigValidator: tick delta mismatch warning");
		tests::Expect(result, config.tick.tickInterval == common::time::Milliseconds(50), "ServerConfigValidator: mismatch keeps tick interval");
		tests::Expect(result, config.tick.fixedDeltaSeconds == 0.033F, "ServerConfigValidator: mismatch keeps fixed delta");
	}

	void RunLoadUdpFaultSimulationConfigTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath =
			tests::MakeTempFilePath(
				"WindowsIocpUdp_UdpFaultSimulationConfig_DebugTest.ini"
			);

		tests::WriteTextFile(
			filePath,
			"[UdpFaultSimulation]\n"
			"Enabled=true\n"
			"DropRate=0.25\n"
			"DuplicateRate=0.5\n"
			"ReorderRate=0.75\n"
			"MinDelayMilliseconds=15\n"
			"MaxDelayMilliseconds=80\n"
			"ReorderDelayMilliseconds=125\n"
			"RandomSeed=123456\n"
		);

		const server::config::ServerConfigLoadResult loadResult =
			server::config::ServerConfigLoader::Load(filePath);

		std::filesystem::remove(filePath);

		tests::Expect(
			result,
			loadResult.loadedFromFile,
			"ServerConfig: UDP fault simulation file loaded"
		);
		tests::Expect(
			result,
			loadResult.warningList.empty(),
			"ServerConfig: valid UDP fault simulation has no warning"
		);

		const common::net::UdpFaultSimulationConfig& config =
			loadResult.config.udpFaultSimulation;

		tests::Expect(
			result,
			config.enabled,
			"ServerConfig: UDP fault simulation enabled"
		);
		tests::Expect(
			result,
			config.dropRate == 0.25F,
			"ServerConfig: UDP fault simulation drop rate"
		);
		tests::Expect(
			result,
			config.duplicateRate == 0.5F,
			"ServerConfig: UDP fault simulation duplicate rate"
		);
		tests::Expect(
			result,
			config.reorderRate == 0.75F,
			"ServerConfig: UDP fault simulation reorder rate"
		);
		tests::Expect(
			result,
			config.minDelay == common::time::Milliseconds(15),
			"ServerConfig: UDP fault simulation minimum delay"
		);
		tests::Expect(
			result,
			config.maxDelay == common::time::Milliseconds(80),
			"ServerConfig: UDP fault simulation maximum delay"
		);
		tests::Expect(
			result,
			config.reorderDelay == common::time::Milliseconds(125),
			"ServerConfig: UDP fault simulation reorder delay"
		);
		tests::Expect(
			result,
			config.randomSeed == 123456,
			"ServerConfig: UDP fault simulation random seed"
		);
	}

	void RunLoadInvalidUdpFaultSimulationConfigTest(
		tests::DebugTestResult& result
	)
	{
		const std::filesystem::path filePath =
			tests::MakeTempFilePath(
				"WindowsIocpUdp_InvalidUdpFaultSimulationConfig_DebugTest.ini"
			);

		tests::WriteTextFile(
			filePath,
			"[UdpFaultSimulation]\n"
			"Enabled=invalid\n"
			"DropRate=-0.1\n"
			"DuplicateRate=1.1\n"
			"ReorderRate=nan\n"
			"MinDelayMilliseconds=-1\n"
			"MaxDelayMilliseconds=invalid\n"
			"ReorderDelayMilliseconds=-1\n"
			"RandomSeed=4294967296\n"
			"UnknownKey=1\n"
		);

		const server::config::ServerConfigLoadResult loadResult =
			server::config::ServerConfigLoader::Load(filePath);

		std::filesystem::remove(filePath);

		const server::config::ServerConfig defaultConfig{};
		const common::net::UdpFaultSimulationConfig& config =
			loadResult.config.udpFaultSimulation;
		const common::net::UdpFaultSimulationConfig& defaultFaultConfig =
			defaultConfig.udpFaultSimulation;

		tests::Expect(
			result,
			loadResult.loadedFromFile,
			"ServerConfig: invalid UDP fault simulation file loaded"
		);
		tests::Expect(
			result,
			loadResult.warningList.size() == 9,
			"ServerConfig: invalid UDP fault simulation warning count"
		);

		tests::Expect(
			result,
			config.enabled == defaultFaultConfig.enabled,
			"ServerConfig: invalid UDP fault enabled ignored"
		);
		tests::Expect(
			result,
			config.dropRate == defaultFaultConfig.dropRate,
			"ServerConfig: invalid UDP fault drop rate ignored"
		);
		tests::Expect(
			result,
			config.duplicateRate == defaultFaultConfig.duplicateRate,
			"ServerConfig: invalid UDP fault duplicate rate ignored"
		);
		tests::Expect(
			result,
			config.reorderRate == defaultFaultConfig.reorderRate,
			"ServerConfig: invalid UDP fault reorder rate ignored"
		);
		tests::Expect(
			result,
			config.minDelay == defaultFaultConfig.minDelay,
			"ServerConfig: invalid UDP fault minimum delay ignored"
		);
		tests::Expect(
			result,
			config.maxDelay == defaultFaultConfig.maxDelay,
			"ServerConfig: invalid UDP fault maximum delay ignored"
		);
		tests::Expect(
			result,
			config.reorderDelay == defaultFaultConfig.reorderDelay,
			"ServerConfig: invalid UDP fault reorder delay ignored"
		);
		tests::Expect(
			result,
			config.randomSeed == defaultFaultConfig.randomSeed,
			"ServerConfig: invalid UDP fault random seed ignored"
		);
	}

	void RunValidatorUdpFaultSimulationNormalizeTest(
		tests::DebugTestResult& result
	)
	{
		server::config::ServerConfig config{};
		const server::config::ServerConfig defaultConfig{};

		config.udpFaultSimulation.dropRate =
			std::numeric_limits<float>::quiet_NaN();
		config.udpFaultSimulation.duplicateRate = -0.1F;
		config.udpFaultSimulation.reorderRate = 1.1F;
		config.udpFaultSimulation.minDelay = common::time::Milliseconds(-1);
		config.udpFaultSimulation.maxDelay = common::time::Milliseconds(-2);
		config.udpFaultSimulation.reorderDelay =
			common::time::Milliseconds(-3);

		const std::vector<server::config::ServerConfigWarning> warningList =
			server::config::ServerConfigValidator::ValidateAndNormalize(config);

		const common::net::UdpFaultSimulationConfig& faultConfig =
			config.udpFaultSimulation;
		const common::net::UdpFaultSimulationConfig& defaultFaultConfig =
			defaultConfig.udpFaultSimulation;

		tests::Expect(
			result,
			faultConfig.dropRate == defaultFaultConfig.dropRate,
			"ServerConfigValidator: UDP fault drop rate normalized"
		);
		tests::Expect(
			result,
			faultConfig.duplicateRate == defaultFaultConfig.duplicateRate,
			"ServerConfigValidator: UDP fault duplicate rate normalized"
		);
		tests::Expect(
			result,
			faultConfig.reorderRate == defaultFaultConfig.reorderRate,
			"ServerConfigValidator: UDP fault reorder rate normalized"
		);
		tests::Expect(
			result,
			faultConfig.minDelay == defaultFaultConfig.minDelay,
			"ServerConfigValidator: UDP fault minimum delay normalized"
		);
		tests::Expect(
			result,
			faultConfig.maxDelay == defaultFaultConfig.maxDelay,
			"ServerConfigValidator: UDP fault maximum delay normalized"
		);
		tests::Expect(
			result,
			faultConfig.reorderDelay == defaultFaultConfig.reorderDelay,
			"ServerConfigValidator: UDP fault reorder delay normalized"
		);

		tests::Expect(
			result,
			tests::ContainsWarningMessage(
				warningList,
				"UdpFaultSimulation.DropRate must be between 0 and 1. "
				"Default value will be used."
			),
			"ServerConfigValidator: UDP fault drop rate warning"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(
				warningList,
				"UdpFaultSimulation.DuplicateRate must be between 0 and 1. "
				"Default value will be used."
			),
			"ServerConfigValidator: UDP fault duplicate rate warning"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(
				warningList,
				"UdpFaultSimulation.ReorderRate must be between 0 and 1. "
				"Default value will be used."
			),
			"ServerConfigValidator: UDP fault reorder rate warning"
		);
	}

	void RunValidatorUdpFaultSimulationDelaySwapTest(
		tests::DebugTestResult& result
	)
	{
		server::config::ServerConfig config{};

		config.udpFaultSimulation.minDelay =
			common::time::Milliseconds(200);
		config.udpFaultSimulation.maxDelay =
			common::time::Milliseconds(50);

		const std::vector<server::config::ServerConfigWarning> warningList =
			server::config::ServerConfigValidator::ValidateAndNormalize(config);

		tests::Expect(
			result,
			config.udpFaultSimulation.minDelay
			== common::time::Milliseconds(50),
			"ServerConfigValidator: UDP fault minimum delay swapped"
		);
		tests::Expect(
			result,
			config.udpFaultSimulation.maxDelay
			== common::time::Milliseconds(200),
			"ServerConfigValidator: UDP fault maximum delay swapped"
		);
		tests::Expect(
			result,
			tests::ContainsWarningMessage(
				warningList,
				"UdpFaultSimulation.MinDelayMs is greater than "
				"MaxDelayMs. Values will be swapped."
			),
			"ServerConfigValidator: UDP fault delay swap warning"
		);
	}

	void RunLoadShortMillisecondsAliasTest(tests::DebugTestResult& result)
	{
		const std::filesystem::path filePath =
			tests::MakeTempFilePath(
				"WindowsIocpUdp_ServerConfig_ShortMillisecondsAlias_DebugTest.ini"
			);

		tests::WriteTextFile(
			filePath,
			"[ReliableUdp]\n"
			"ResendIntervalMs=123\n"
			"\n"
			"[UdpFaultSimulation]\n"
			"MinDelayMs=10\n"
			"MaxDelayMs=50\n"
			"ReorderDelayMs=30\n"
		);

		const server::config::ServerConfigLoadResult loadResult =
			server::config::ServerConfigLoader::Load(filePath);

		std::filesystem::remove(filePath);

		tests::Expect(
			result,
			loadResult.loadedFromFile,
			"ServerConfig: short millisecond alias file loaded"
		);
		tests::Expect(
			result,
			loadResult.warningList.empty(),
			"ServerConfig: short millisecond alias has no loader warning"
		);
		tests::Expect(
			result,
			loadResult.config.reliableUdp.resendInterval == common::time::Milliseconds(123),
			"ServerConfig: ReliableUdp.ResendIntervalMs alias parsed"
		);
		tests::Expect(
			result,
			loadResult.config.udpFaultSimulation.minDelay == common::time::Milliseconds(10),
			"ServerConfig: UdpFaultSimulation.MinDelayMs alias parsed"
		);
		tests::Expect(
			result,
			loadResult.config.udpFaultSimulation.maxDelay == common::time::Milliseconds(50),
			"ServerConfig: UdpFaultSimulation.MaxDelayMs alias parsed"
		);
		tests::Expect(
			result,
			loadResult.config.udpFaultSimulation.reorderDelay == common::time::Milliseconds(30),
			"ServerConfig: UdpFaultSimulation.ReorderDelayMs alias parsed"
		);
	}
}

namespace tests::server
{
	tests::DebugTestResult RunServerConfigTests()
	{
		tests::DebugTestResult result{};

		RunLoadValidConfigTest(result);
		RunLoadInvalidConfigTest(result);
		RunLoadLogLevelAliasTest(result);
		RunLoadShortMillisecondsAliasTest(result);
		RunLoadUdpFaultSimulationConfigTest(result);
		RunLoadInvalidUdpFaultSimulationConfigTest(result);
		RunMissingFileTest(result);
		RunValidatorNormalizeTest(result);
		RunValidatorUdpFaultSimulationNormalizeTest(result);
		RunValidatorUdpFaultSimulationDelaySwapTest(result);
		RunValidatorTickDeltaMismatchWarningTest(result);

		return result;
	}
}
