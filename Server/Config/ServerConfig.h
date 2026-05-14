#pragma once

#include <chrono>
#include <cstddef>

#include <Common/Game/GameRules.h>
#include <Common/Game/GameTypes.h>
#include <Common/Game/SimulationConstants.h>
#include <Common/Game/WeaponRules.h>

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
		std::chrono::seconds peerTimeout = std::chrono::seconds(10);
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

	struct DiagnosticsConfig
	{
	public:
		bool enableStatusLog = true;
		std::chrono::seconds statusLogInterval = std::chrono::seconds(10);
	};

	struct ServerConfig
	{
	public:
		NetworkConfig network;
		SessionConfig session;
		TickConfig tick;
		GameRuleConfig gameRule;
		WeaponRuleConfig weaponRule;
		DiagnosticsConfig diagnostics;
	};
}