#pragma once

#include <chrono>
#include <string_view>

#include <Common/Game/SimulationConstants.h>

namespace client::config
{
	inline constexpr std::string_view defaultServerIp = "127.0.0.1";
	inline constexpr unsigned short defaultServerPort = 9000;

	inline constexpr std::chrono::milliseconds defaultUpdateSleepInterval = std::chrono::milliseconds(1);
	inline constexpr std::chrono::milliseconds defaultJoinRetryInterval = std::chrono::milliseconds(1000);
	inline constexpr std::chrono::milliseconds defaultRoomJoinInterval = std::chrono::milliseconds(200);
	inline constexpr std::chrono::milliseconds defaultInterpolationAdjustStep = std::chrono::milliseconds(10);

	inline constexpr std::chrono::milliseconds defaultInterpolationDelay = std::chrono::milliseconds(100);
	inline constexpr std::chrono::milliseconds minInterpolationDelay = std::chrono::milliseconds(0);
	inline constexpr std::chrono::milliseconds maxInterpolationDelay = std::chrono::milliseconds(500);

	inline constexpr std::chrono::milliseconds defaultSnapshotAssemblyTimeout = std::chrono::milliseconds(500);

	inline constexpr bool defaultEnableChunkAssemblerDebugTests = true;

	inline constexpr std::chrono::milliseconds defaultSimulationTickInterval = common::game::defaultFixedTickInterval;
	inline constexpr float defaultSimulationDeltaSeconds = common::game::defaultFixedDeltaSeconds;
}