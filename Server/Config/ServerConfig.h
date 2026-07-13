#pragma once

#include <chrono>
#include <cstddef>
#include <string>

#include <Common/Game/GameRules.h>
#include <Common/Game/GameTypes.h>
#include <Common/Game/SimulationConstants.h>
#include <Common/Game/WeaponRules.h>
#include <Common/Log/LogLevel.h>
#include <Common/Net/Fault/UdpFaultSimulationConfig.h>
#include <Common/Time/TimeTypes.h>

namespace server::config
{
	struct NetworkConfig
	{
	public:
		unsigned short port = 9000;
		std::size_t workerThreadCount = 0;
		std::size_t recvContextCount = 0;
	};

	struct SessionConfig
	{
	public:
		common::game::RoomId initialRoomId = 1;
		common::time::Seconds peerTimeout = std::chrono::seconds(10);
	};

	struct ReliableUdpConfig
	{
	public:
		std::size_t maxPendingPacketCount = 64;
		int maxResendCount = 10;
		common::time::Milliseconds resendInterval = common::time::Milliseconds(100);
	};

	struct TickConfig
	{
	public:
		std::chrono::steady_clock::duration tickInterval = common::game::defaultFixedTickInterval;
		float fixedDeltaSeconds = common::game::defaultFixedDeltaSeconds;
	};

	struct GameRuleConfig
	{
	public:
		int initialPlayerHp = common::game::defaultInitialPlayerHp;
		float respawnDelaySeconds = common::game::defaultRespawnDelaySeconds;
		float respawnInvincibilitySeconds = common::game::defaultRespawnInvincibilitySeconds;
		float hitFlashDurationSeconds = common::game::defaultHitFlashDurationSeconds;
	};

	struct WeaponRuleConfig
	{
	public:
		common::game::WeaponRule basicWeaponRule = common::game::defaultBasicWeaponRule;
	};

	struct DatabaseConfig
	{
	public:
		bool enabled = false;
		std::string connectionString;
		int connectionTimeoutSeconds = 5;
	};

	struct DiagnosticsConfig
	{
	public:
		bool enableStatusLog = true;
		std::chrono::seconds statusLogInterval = std::chrono::seconds(10);
		common::log::LogLevel logLevel = common::log::LogLevel::Info;
		std::size_t asyncLogWorkerThreadCount = 1;
	};

	struct ServerConfig
	{
	public:
		NetworkConfig network;
		SessionConfig session;
		ReliableUdpConfig reliableUdp;
		common::net::UdpFaultSimulationConfig udpFaultSimulation;
		TickConfig tick;
		GameRuleConfig gameRule;
		WeaponRuleConfig weaponRule;
		DatabaseConfig database;
		DiagnosticsConfig diagnostics;
	};
}