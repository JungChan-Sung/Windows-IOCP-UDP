#pragma once

#include <cstddef>
#include <string>

#include <Common/Game/GameRules.h>
#include <Common/Game/GameTypes.h>
#include <Common/Game/SimulationConstants.h>
#include <Common/Game/WeaponRules.h>
#include <Common/Log/LogLevel.h>
#include <Common/Net/Fault/UdpFaultSimulationConfig.h>
#include <Common/Net/Reliable/ReliableUdpConfig.h>
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
		common::time::Seconds peerTimeout = common::time::Seconds(10);
		common::time::Seconds reconnectGracePeriod = common::time::Seconds(30);
	};

	struct TickConfig
	{
	public:
		common::time::Duration tickInterval = common::game::defaultFixedTickInterval;
		float fixedDeltaSeconds = common::game::defaultFixedDeltaSeconds;
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
		common::time::Seconds statusLogInterval = common::time::Seconds(10);
		common::log::LogLevel logLevel = common::log::LogLevel::Info;
		std::size_t asyncLogWorkerThreadCount = 1;
	};

	struct ServerConfig
	{
	public:
		NetworkConfig network;
		SessionConfig session;
		common::net::ReliableUdpConfig reliableUdp;
		common::net::UdpFaultSimulationConfig udpFaultSimulation;
		TickConfig tick;
		common::game::GameRuleConfig gameRule;
		common::game::WeaponRuleConfig weaponRule;
		DatabaseConfig database;
		DiagnosticsConfig diagnostics;
	};
}