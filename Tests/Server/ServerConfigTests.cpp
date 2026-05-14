#include "ServerConfigTests.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <Server/Config/ServerConfigLoader.h>
#include <Server/Config/ServerConfigValidator.h>

namespace
{
	[[nodiscard]] std::filesystem::path MakeTempServerConfigPath()
	{
		return std::filesystem::temp_directory_path() / "WindowsIocpUdp_ServerConfig_DebugTest.ini";
	}

	void WriteTextFile(const std::filesystem::path& filePath, const std::string& text)
	{
		std::ofstream file(filePath, std::ios::trunc);
		file << text;
	}

	void RunLoadValidConfigTest(common::diagnostics::DebugTestResult& result)
	{
		const std::filesystem::path filePath = MakeTempServerConfigPath();

		WriteTextFile(
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
		);

		const server::config::ServerConfigLoadResult loadResult = server::config::ServerConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		common::diagnostics::Expect(result, loadResult.loadedFromFile, "ServerConfig: valid file loaded");
		common::diagnostics::Expect(result, loadResult.warningList.empty(), "ServerConfig: valid file has no loader warning");

		const server::config::ServerConfig& config = loadResult.config;

		common::diagnostics::Expect(result, config.network.port == 9100, "ServerConfig: port");
		common::diagnostics::Expect(result, config.network.workerThreadCount == 2, "ServerConfig: workerThreadCount");
		common::diagnostics::Expect(result, config.network.recvContextCount == 64, "ServerConfig: recvContextCount");
		common::diagnostics::Expect(result, config.session.initialRoomId == 3, "ServerConfig: initialRoomId");
		common::diagnostics::Expect(result, config.session.peerTimeout == std::chrono::seconds(15), "ServerConfig: peerTimeout");
		common::diagnostics::Expect(result, config.tick.tickInterval == std::chrono::milliseconds(33), "ServerConfig: tickInterval");
		common::diagnostics::Expect(result, config.tick.fixedDeltaSeconds == 0.033F, "ServerConfig: fixedDeltaSeconds");
		common::diagnostics::Expect(result, config.gameRule.initialPlayerHp == 5, "ServerConfig: initialPlayerHp");
		common::diagnostics::Expect(result, config.gameRule.respawnDelaySeconds == 2.5F, "ServerConfig: respawnDelaySeconds");
		common::diagnostics::Expect(result, config.gameRule.respawnInvincibilitySeconds == 1.5F, "ServerConfig: respawnInvincibilitySeconds");
		common::diagnostics::Expect(result, config.gameRule.hitFlashDurationSeconds == 0.25F, "ServerConfig: hitFlashDurationSeconds");
		common::diagnostics::Expect(result, config.weaponRule.basicWeaponRule.bulletDamage == 2, "ServerConfig: bulletDamage");
		common::diagnostics::Expect(result, config.weaponRule.basicWeaponRule.bulletSpeed == 700.0F, "ServerConfig: bulletSpeed");
		common::diagnostics::Expect(result, config.weaponRule.basicWeaponRule.bulletLifeSeconds == 2.0F, "ServerConfig: bulletLifeSeconds");
		common::diagnostics::Expect(result, config.weaponRule.basicWeaponRule.bulletRadius == 7.5F, "ServerConfig: bulletRadius");
		common::diagnostics::Expect(result, config.weaponRule.basicWeaponRule.fireCooldownSeconds == 0.2F, "ServerConfig: fireCooldownSeconds");
		common::diagnostics::Expect(result, !config.diagnostics.enableStatusLog, "ServerConfig: enableStatusLog");
		common::diagnostics::Expect(result, config.diagnostics.statusLogInterval == std::chrono::seconds(20), "ServerConfig: statusLogInterval");
	}

	void RunLoadInvalidConfigTest(common::diagnostics::DebugTestResult& result)
	{
		const std::filesystem::path filePath = MakeTempServerConfigPath();

		WriteTextFile(
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
			"[Tick]\n"
			"TickIntervalMs=0\n"
			"FixedDeltaSeconds=bad\n"
		);

		const server::config::ServerConfigLoadResult loadResult = server::config::ServerConfigLoader::Load(filePath);
		std::filesystem::remove(filePath);

		common::diagnostics::Expect(result, loadResult.loadedFromFile, "ServerConfig: invalid file loaded");
		common::diagnostics::Expect(result, loadResult.warningList.size() >= 5, "ServerConfig: invalid file warning count");
	}

	void RunMissingFileTest(common::diagnostics::DebugTestResult& result)
	{
		const std::filesystem::path filePath = MakeTempServerConfigPath();
		std::filesystem::remove(filePath);

		const server::config::ServerConfigLoadResult loadResult = server::config::ServerConfigLoader::Load(filePath);

		common::diagnostics::Expect(result, !loadResult.loadedFromFile, "ServerConfig: missing file not loaded");
		common::diagnostics::Expect(result, !loadResult.warningList.empty(), "ServerConfig: missing file warning");
	}

	void RunValidatorNormalizeTest(common::diagnostics::DebugTestResult& result)
	{
		server::config::ServerConfig config{};
		const server::config::ServerConfig defaultConfig{};

		config.network.port = 0;
		config.session.initialRoomId = 0;
		config.session.peerTimeout = std::chrono::seconds(0);
		config.tick.tickInterval = std::chrono::milliseconds(0);
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

		const std::vector<server::config::ServerConfigWarning> warningList = server::config::ServerConfigValidator::ValidateAndNormalize(config);

		common::diagnostics::Expect(result, !warningList.empty(), "ServerConfigValidator: warning generated");
		common::diagnostics::Expect(result, config.network.port == defaultConfig.network.port, "ServerConfigValidator: port normalized");
		common::diagnostics::Expect(result, config.network.workerThreadCount > 0, "ServerConfigValidator: worker count resolved");
		common::diagnostics::Expect(result, config.network.recvContextCount > 0, "ServerConfigValidator: recv context count resolved");
		common::diagnostics::Expect(result, config.session.initialRoomId == defaultConfig.session.initialRoomId, "ServerConfigValidator: room normalized");
		common::diagnostics::Expect(result, config.session.peerTimeout == defaultConfig.session.peerTimeout, "ServerConfigValidator: timeout normalized");
		common::diagnostics::Expect(result, config.tick.tickInterval == defaultConfig.tick.tickInterval, "ServerConfigValidator: tick interval normalized");
		common::diagnostics::Expect(result, config.tick.fixedDeltaSeconds == defaultConfig.tick.fixedDeltaSeconds, "ServerConfigValidator: delta normalized");
		common::diagnostics::Expect(result, config.gameRule.initialPlayerHp == defaultConfig.gameRule.initialPlayerHp, "ServerConfigValidator: hp normalized");
		common::diagnostics::Expect(result, config.weaponRule.basicWeaponRule.bulletDamage == defaultConfig.weaponRule.basicWeaponRule.bulletDamage,
			"ServerConfigValidator: damage normalized");
		common::diagnostics::Expect(result, config.diagnostics.statusLogInterval == defaultConfig.diagnostics.statusLogInterval,
			"ServerConfigValidator: status interval normalized");
	}
}

namespace tests::server
{
	common::diagnostics::DebugTestResult RunServerConfigTests()
	{
		common::diagnostics::DebugTestResult result{};

		RunLoadValidConfigTest(result);
		RunLoadInvalidConfigTest(result);
		RunMissingFileTest(result);
		RunValidatorNormalizeTest(result);

		return result;
	}
}
