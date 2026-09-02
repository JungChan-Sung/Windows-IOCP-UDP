#include "ServerConfigDatabaseApplierTests.h"

#include <chrono>
#include <vector>

#include <Common/Log/LogLevel.h>

#include <Persistence/Config/ServerConfigRepository.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Config/ServerConfigDatabaseApplier.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void RunValidOverrideTest(tests::DebugTestResult& result)
	{
		server::config::ServerConfig serverConfig{};

		const std::vector<persistence::config::ServerConfigEntry> entryList{
			{
				.key = "Session.PeerTimeoutSeconds",
				.value = "25",
			},
			{
				.key = "Session.ReconnectGracePeriodSeconds",
				.value = "60",
			},
			{
				.key = "ReliableUdp.ResendIntervalMs",
				.value = "150",
			},
			{
				.key = "GameRule.InitialPlayerHp",
				.value = "9",
			},
			{
				.key = "Weapon.Basic.BulletDamage",
				.value = "4",
			},
			{
				.key = "Diagnostics.LogLevel",
				.value = "Debug",
			},
		};

		const server::config::ServerConfigDatabaseApplyResult applyResult
			= server::config::ServerConfigDatabaseApplier::Apply(
				serverConfig,
				entryList
			);

		tests::Expect(
			result,
			applyResult.appliedCount == entryList.size(),
			"ServerConfigDatabaseApplier: all valid entries applied"
		);

		tests::Expect(
			result,
			applyResult.warningList.empty(),
			"ServerConfigDatabaseApplier: valid entries have no warnings"
		);

		tests::Expect(
			result,
			serverConfig.session.peerTimeout == std::chrono::seconds(25),
			"ServerConfigDatabaseApplier: peer timeout overridden"
		);

		tests::Expect(
			result,
			serverConfig.session.reconnectGracePeriod == std::chrono::seconds(60),
			"ServerConfigDatabaseApplier: reconnect grace period overridden"
		);

		tests::Expect(
			result,
			serverConfig.reliableUdp.resendInterval
			== common::time::Milliseconds(150),
			"ServerConfigDatabaseApplier: reliable resend interval overridden"
		);

		tests::Expect(
			result,
			serverConfig.gameRule.initialPlayerHp == 9,
			"ServerConfigDatabaseApplier: player hp overridden"
		);

		tests::Expect(
			result,
			serverConfig.weaponRule.basicWeaponRule.bulletDamage == 4,
			"ServerConfigDatabaseApplier: bullet damage overridden"
		);

		tests::Expect(
			result,
			serverConfig.diagnostics.logLevel
			== common::log::LogLevel::Debug,
			"ServerConfigDatabaseApplier: log level overridden"
		);
	}

	void RunBootstrapSettingRejectedTest(
		tests::DebugTestResult& result
	)
	{
		server::config::ServerConfig serverConfig{};

		const unsigned short originalPort = serverConfig.network.port;
		const bool originalDatabaseEnabled = serverConfig.database.enabled;

		const std::vector<persistence::config::ServerConfigEntry> entryList{
			{
				.key = "Network.Port",
				.value = "9999",
			},
			{
				.key = "Database.Enabled",
				.value = "true",
			},
		};

		const server::config::ServerConfigDatabaseApplyResult applyResult
			= server::config::ServerConfigDatabaseApplier::Apply(
				serverConfig,
				entryList
			);

		tests::Expect(
			result,
			applyResult.appliedCount == 0,
			"ServerConfigDatabaseApplier: bootstrap settings not applied"
		);

		tests::Expect(
			result,
			applyResult.warningList.size() == 2,
			"ServerConfigDatabaseApplier: bootstrap settings generate warnings"
		);

		tests::Expect(
			result,
			serverConfig.network.port == originalPort,
			"ServerConfigDatabaseApplier: network port preserved"
		);

		tests::Expect(
			result,
			serverConfig.database.enabled == originalDatabaseEnabled,
			"ServerConfigDatabaseApplier: database enabled preserved"
		);
	}

	void RunInvalidEntryTest(tests::DebugTestResult& result)
	{
		server::config::ServerConfig serverConfig{};

		const common::time::Seconds originalPeerTimeout
			= serverConfig.session.peerTimeout;

		const std::vector<persistence::config::ServerConfigEntry> entryList{
			{
				.key = "InvalidKey",
				.value = "10",
			},
			{
				.key = "Session.PeerTimeoutSeconds",
				.value = "0",
			},
			{
				.key = "Unknown.Value",
				.value = "1",
			},
		};

		const server::config::ServerConfigDatabaseApplyResult applyResult
			= server::config::ServerConfigDatabaseApplier::Apply(
				serverConfig,
				entryList
			);

		tests::Expect(
			result,
			applyResult.appliedCount == 0,
			"ServerConfigDatabaseApplier: invalid entries not applied"
		);

		tests::Expect(
			result,
			applyResult.warningList.size() == 3,
			"ServerConfigDatabaseApplier: invalid entries generate warnings"
		);

		tests::Expect(
			result,
			serverConfig.session.peerTimeout == originalPeerTimeout,
			"ServerConfigDatabaseApplier: invalid value preserves previous value"
		);
	}

	void RunIniBaseOverrideTest(tests::DebugTestResult& result)
	{
		server::config::ServerConfig serverConfig{};
		serverConfig.session.peerTimeout = std::chrono::seconds(17);
		serverConfig.gameRule.initialPlayerHp = 6;

		const std::vector<persistence::config::ServerConfigEntry> entryList{
			{
				.key = "Session.PeerTimeoutSeconds",
				.value = "33",
			},
			{
				.key = "GameRule.InitialPlayerHp",
				.value = "12",
			},
		};

		const server::config::ServerConfigDatabaseApplyResult applyResult
			= server::config::ServerConfigDatabaseApplier::Apply(
				serverConfig,
				entryList
			);

		tests::Expect(
			result,
			applyResult.appliedCount == 2,
			"ServerConfigDatabaseApplier: database values override base config"
		);

		tests::Expect(
			result,
			serverConfig.session.peerTimeout == std::chrono::seconds(33),
			"ServerConfigDatabaseApplier: database timeout wins over base value"
		);

		tests::Expect(
			result,
			serverConfig.gameRule.initialPlayerHp == 12,
			"ServerConfigDatabaseApplier: database game rule wins over base value"
		);
	}
}

namespace tests::server
{
	DebugTestResult RunServerConfigDatabaseApplierTests()
	{
		DebugTestResult result{};

		RunValidOverrideTest(result);
		RunBootstrapSettingRejectedTest(result);
		RunInvalidEntryTest(result);
		RunIniBaseOverrideTest(result);

		return result;
	}
}