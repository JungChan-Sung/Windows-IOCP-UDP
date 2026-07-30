#pragma once

#include <cstddef>
#include <string_view>

#include <Common/Game/SimulationConstants.h>
#include <Common/Log/LogLevel.h>
#include <Common/Time/TimeTypes.h>

namespace client::config
{
	inline constexpr std::string_view defaultServerIp = "127.0.0.1";
	inline constexpr unsigned short defaultServerPort = 9000;

	inline constexpr std::string_view defaultAccountLoginName = "";
	inline constexpr std::string_view defaultAccountPasswordHash = "";

	inline constexpr common::time::Milliseconds defaultUpdateSleepInterval = common::time::Milliseconds(1);
	inline constexpr common::time::Milliseconds defaultAccountLoginRetryInterval = common::time::Milliseconds(1000);
	inline constexpr common::time::Milliseconds defaultJoinRetryInterval = common::time::Milliseconds(1000);
	inline constexpr common::time::Milliseconds defaultRoomJoinInterval = common::time::Milliseconds(200);
	inline constexpr common::time::Milliseconds defaultInterpolationAdjustStep = common::time::Milliseconds(10);

	inline constexpr common::time::Milliseconds defaultInterpolationDelay = common::time::Milliseconds(100);
	inline constexpr common::time::Milliseconds minInterpolationDelay = common::time::Milliseconds(0);
	inline constexpr common::time::Milliseconds maxInterpolationDelay = common::time::Milliseconds(500);

	inline constexpr common::time::Milliseconds defaultSnapshotAssemblyTimeout = common::time::Milliseconds(500);

	inline constexpr common::log::LogLevel defaultLogLevel = common::log::LogLevel::Info;
	inline constexpr std::size_t defaultAsyncLogWorkerThreadCount = 1;

	inline constexpr common::time::Milliseconds defaultSimulationTickInterval = common::game::defaultFixedTickInterval;
	inline constexpr float defaultSimulationDeltaSeconds = common::game::defaultFixedDeltaSeconds;

	inline constexpr std::size_t defaultIocpWorkerThreadCount = 1;
	inline constexpr std::size_t defaultIocpRecvContextCount = 4;
}