#include "ServerConfigValidator.h"

#include <cmath>
#include <chrono>
#include <sstream>
#include <string>
#include <utility>
#include <thread>

namespace
{
	void AddWarning(std::vector<server::config::ServerConfigWarning>& warningList, std::string message)
	{
		server::config::ServerConfigWarning warning{};
		warning.lineNumber = 0;
		warning.message = std::move(message);

		warningList.push_back(std::move(warning));
	}

	[[nodiscard]] float GetDurationSeconds(std::chrono::steady_clock::duration duration) noexcept
	{
		return std::chrono::duration<float>(duration).count();
	}

	[[nodiscard]] std::size_t ResolveWorkerThreadCount(std::size_t requestedWorkerThreadCount) noexcept
	{
		if (requestedWorkerThreadCount != 0)
		{
			return requestedWorkerThreadCount;
		}

		const unsigned int hardwareThreadCount = std::thread::hardware_concurrency();
		if (hardwareThreadCount == 0)
		{
			return 1;
		}

		return static_cast<std::size_t>(hardwareThreadCount);
	}

	[[nodiscard]] std::size_t ResolveRecvContextCount(std::size_t requestedRecvContextCount, std::size_t workerThreadCount) noexcept
	{
		if (requestedRecvContextCount != 0)
		{
			return requestedRecvContextCount;
		}

		const std::size_t recvContextCount = workerThreadCount * 2;
		return (recvContextCount != 0) ? recvContextCount : 1;
	}
}

namespace server::config
{
	std::vector<ServerConfigWarning> ServerConfigValidator::ValidateAndNormalize(ServerConfig& serverConfig)
	{
		std::vector<ServerConfigWarning> warningList;
		const ServerConfig defaultConfig{};

		if (serverConfig.network.port == 0)
		{
			AddWarning(warningList, "Network.Port cannot be 0. Default port will be used.");
			serverConfig.network.port = defaultConfig.network.port;
		}

		serverConfig.network.workerThreadCount = ResolveWorkerThreadCount(serverConfig.network.workerThreadCount);
		serverConfig.network.recvContextCount = ResolveRecvContextCount(
			serverConfig.network.recvContextCount,
			serverConfig.network.workerThreadCount
		);

		if (serverConfig.session.initialRoomId <= 0)
		{
			AddWarning(warningList, "Session.InitialRoomId must be greater than 0. Default room id will be used.");
			serverConfig.session.initialRoomId = defaultConfig.session.initialRoomId;
		}

		if (serverConfig.session.peerTimeout <= std::chrono::seconds(0))
		{
			AddWarning(warningList, "Session.PeerTimeoutSeconds must be greater than 0. Default timeout will be used.");
			serverConfig.session.peerTimeout = defaultConfig.session.peerTimeout;
		}

		if (serverConfig.reliableUdp.maxPendingPacketCount == 0)
		{
			AddWarning(
				warningList,
				"ReliableUdp.MaxPendingPacketCount must be greater than 0. Default max pending packet count will be used."
			);
			serverConfig.reliableUdp.maxPendingPacketCount = defaultConfig.reliableUdp.maxPendingPacketCount;
		}

		if (serverConfig.reliableUdp.maxResendCount < 0)
		{
			AddWarning(
				warningList,
				"ReliableUdp.MaxResendCount must be greater than or equal to 0. Default max resend count will be used."
			);
			serverConfig.reliableUdp.maxResendCount = defaultConfig.reliableUdp.maxResendCount;
		}

		if (serverConfig.reliableUdp.resendIntervalMilliseconds <= 0)
		{
			AddWarning(
				warningList,
				"ReliableUdp.ResendIntervalMs must be greater than 0. Default resend interval will be used."
			);
			serverConfig.reliableUdp.resendIntervalMilliseconds = defaultConfig.reliableUdp.resendIntervalMilliseconds;
		}

		if (!std::isfinite(serverConfig.udpFaultSimulation.dropRate)
			|| serverConfig.udpFaultSimulation.dropRate < 0.0F
			|| serverConfig.udpFaultSimulation.dropRate > 1.0F)
		{
			AddWarning(
				warningList,
				"UdpFaultSimulation.DropRate must be between 0 and 1. Default value will be used."
			);

			serverConfig.udpFaultSimulation.dropRate = defaultConfig.udpFaultSimulation.dropRate;
		}

		if (!std::isfinite(serverConfig.udpFaultSimulation.duplicateRate)
			|| serverConfig.udpFaultSimulation.duplicateRate < 0.0F
			|| serverConfig.udpFaultSimulation.duplicateRate > 1.0F)
		{
			AddWarning(
				warningList,
				"UdpFaultSimulation.DuplicateRate must be between 0 and 1. Default value will be used."
			);

			serverConfig.udpFaultSimulation.duplicateRate = defaultConfig.udpFaultSimulation.duplicateRate;
		}

		if (!std::isfinite(serverConfig.udpFaultSimulation.reorderRate)
			|| serverConfig.udpFaultSimulation.reorderRate < 0.0F
			|| serverConfig.udpFaultSimulation.reorderRate > 1.0F)
		{
			AddWarning(
				warningList,
				"UdpFaultSimulation.ReorderRate must be between 0 and 1. Default value will be used."
			);

			serverConfig.udpFaultSimulation.reorderRate = defaultConfig.udpFaultSimulation.reorderRate;
		}

		if (serverConfig.udpFaultSimulation.minDelay < std::chrono::milliseconds::zero())
		{
			AddWarning(
				warningList,
				"UdpFaultSimulation.MinDelayMs must be greater than or equal to 0. Default value will be used."
			);

			serverConfig.udpFaultSimulation.minDelay = defaultConfig.udpFaultSimulation.minDelay;
		}

		if (serverConfig.udpFaultSimulation.maxDelay < std::chrono::milliseconds::zero())
		{
			AddWarning(
				warningList,
				"UdpFaultSimulation.MaxDelayMs must be greater than or equal to 0. Default value will be used."
			);

			serverConfig.udpFaultSimulation.maxDelay = defaultConfig.udpFaultSimulation.maxDelay;
		}

		if (serverConfig.udpFaultSimulation.reorderDelay < std::chrono::milliseconds::zero())
		{
			AddWarning(
				warningList,
				"UdpFaultSimulation.ReorderDelayMs must be greater than or equal to 0. Default value will be used."
			);

			serverConfig.udpFaultSimulation.reorderDelay = defaultConfig.udpFaultSimulation.reorderDelay;
		}

		if (serverConfig.udpFaultSimulation.minDelay > serverConfig.udpFaultSimulation.maxDelay)
		{
			AddWarning(
				warningList,
				"UdpFaultSimulation.MinDelayMs is greater than MaxDelayMs. Values will be swapped."
			);

			std::swap(
				serverConfig.udpFaultSimulation.minDelay,
				serverConfig.udpFaultSimulation.maxDelay
			);
		}

		if (serverConfig.tick.tickInterval <= std::chrono::steady_clock::duration::zero())
		{
			AddWarning(warningList, "Tick.TickIntervalMs must be greater than 0. Default tick interval will be used.");
			serverConfig.tick.tickInterval = defaultConfig.tick.tickInterval;
		}

		if (serverConfig.tick.fixedDeltaSeconds <= 0.0F)
		{
			AddWarning(warningList, "Tick.FixedDeltaSeconds must be greater than 0. Default delta will be used.");
			serverConfig.tick.fixedDeltaSeconds = defaultConfig.tick.fixedDeltaSeconds;
		}

		const float tickIntervalSeconds = GetDurationSeconds(serverConfig.tick.tickInterval);
		const float deltaDifference = std::abs(tickIntervalSeconds - serverConfig.tick.fixedDeltaSeconds);

		if (serverConfig.gameRule.initialPlayerHp <= 0)
		{
			AddWarning(warningList, "GameRule.InitialPlayerHp must be greater than 0. Default hp will be used.");
			serverConfig.gameRule.initialPlayerHp = defaultConfig.gameRule.initialPlayerHp;
		}

		if (serverConfig.gameRule.respawnDelaySeconds < 0.0F)
		{
			AddWarning(
				warningList,
				"GameRule.RespawnDelaySeconds must be greater than or equal to 0. Default respawn delay will be used."
			);
			serverConfig.gameRule.respawnDelaySeconds = defaultConfig.gameRule.respawnDelaySeconds;
		}

		if (serverConfig.gameRule.respawnInvincibilitySeconds < 0.0F)
		{
			AddWarning(
				warningList,
				"GameRule.RespawnInvincibilitySeconds must be greater than or equal to 0. Default invincibility time will be used."
			);
			serverConfig.gameRule.respawnInvincibilitySeconds = defaultConfig.gameRule.respawnInvincibilitySeconds;
		}

		if (serverConfig.gameRule.hitFlashDurationSeconds < 0.0F)
		{
			AddWarning(
				warningList,
				"GameRule.HitFlashDurationSeconds must be greater than or equal to 0. Default hit flash duration will be used."
			);
			serverConfig.gameRule.hitFlashDurationSeconds = defaultConfig.gameRule.hitFlashDurationSeconds;
		}

		if (serverConfig.weaponRule.basicWeaponRule.bulletDamage <= 0)
		{
			AddWarning(warningList, "Weapon.Basic.BulletDamage must be greater than 0. Default damage will be used.");
			serverConfig.weaponRule.basicWeaponRule.bulletDamage = defaultConfig.weaponRule.basicWeaponRule.bulletDamage;
		}

		if (serverConfig.weaponRule.basicWeaponRule.bulletSpeed <= 0.0F)
		{
			AddWarning(warningList, "Weapon.Basic.BulletSpeed must be greater than 0. Default speed will be used.");
			serverConfig.weaponRule.basicWeaponRule.bulletSpeed = defaultConfig.weaponRule.basicWeaponRule.bulletSpeed;
		}

		if (serverConfig.weaponRule.basicWeaponRule.bulletLifeSeconds <= 0.0F)
		{
			AddWarning(warningList, "Weapon.Basic.BulletLifeSeconds must be greater than 0. Default life time will be used.");
			serverConfig.weaponRule.basicWeaponRule.bulletLifeSeconds
				= defaultConfig.weaponRule.basicWeaponRule.bulletLifeSeconds;
		}

		if (serverConfig.weaponRule.basicWeaponRule.bulletRadius <= 0.0F)
		{
			AddWarning(warningList, "Weapon.Basic.BulletRadius must be greater than 0. Default radius will be used.");
			serverConfig.weaponRule.basicWeaponRule.bulletRadius = defaultConfig.weaponRule.basicWeaponRule.bulletRadius;
		}

		if (serverConfig.weaponRule.basicWeaponRule.fireCooldownSeconds < 0.0F)
		{
			AddWarning(
				warningList,
				"Weapon.Basic.FireCooldownSeconds must be greater than or equal to 0. Default cooldown will be used."
			);
			serverConfig.weaponRule.basicWeaponRule.fireCooldownSeconds
				= defaultConfig.weaponRule.basicWeaponRule.fireCooldownSeconds;
		}

		if (serverConfig.diagnostics.statusLogInterval <= std::chrono::seconds(0))
		{
			AddWarning(warningList, "Diagnostics.StatusLogIntervalSeconds must be greater than 0. Default status log interval will be used.");
			serverConfig.diagnostics.statusLogInterval = defaultConfig.diagnostics.statusLogInterval;
		}

		if (serverConfig.diagnostics.asyncLogWorkerThreadCount == 0)
		{
			AddWarning(warningList, "Diagnostics.AsyncLogWorkerThreadCount cannot be 0. Default value will be used.");
			serverConfig.diagnostics.asyncLogWorkerThreadCount = defaultConfig.diagnostics.asyncLogWorkerThreadCount;
		}

		if (deltaDifference > 0.001F)
		{
			std::ostringstream stream;
			stream << "Tick.FixedDeltaSeconds does not match Tick.TickIntervalMs. "
				<< "TickIntervalSeconds=" << tickIntervalSeconds
				<< ", FixedDeltaSeconds=" << serverConfig.tick.fixedDeltaSeconds
				<< ". Simulation will use FixedDeltaSeconds.";

			AddWarning(warningList, stream.str());
		}

		return warningList;
	}
}