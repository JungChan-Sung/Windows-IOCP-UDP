#pragma once

#include <chrono>
#include <string>

#include <Client/Config/ClientConfigDefaults.h>

namespace client::config
{
	struct NetworkConfig
	{
	public:
		std::string serverIp = std::string(defaultServerIp);
		unsigned short serverPort = defaultServerPort;
	};

	struct TimingConfig
	{
	public:
		std::chrono::milliseconds updateSleepInterval = defaultUpdateSleepInterval;
		std::chrono::milliseconds joinRetryInterval = defaultJoinRetryInterval;
		std::chrono::milliseconds roomJoinInterval = defaultRoomJoinInterval;
		std::chrono::milliseconds interpolationAdjustStep = defaultInterpolationAdjustStep;
	};

	struct InterpolationConfig
	{
	public:
		std::chrono::milliseconds defaultDelay = defaultInterpolationDelay;
		std::chrono::milliseconds minDelay = minInterpolationDelay;
		std::chrono::milliseconds maxDelay = maxInterpolationDelay;
	};

	struct SnapshotConfig
	{
	public:
		std::chrono::milliseconds assemblyTimeout = defaultSnapshotAssemblyTimeout;
	};

	struct SimulationConfig
	{
	public:
		std::chrono::milliseconds tickInterval = defaultSimulationTickInterval;
		float deltaSeconds = defaultSimulationDeltaSeconds;
	};

	struct DiagnosticsConfig
	{
	public:
		bool enableChunkAssemblerDebugTests = defaultEnableChunkAssemblerDebugTests;
	};

	struct ClientConfig
	{
	public:
		NetworkConfig network;
		TimingConfig timing;
		InterpolationConfig interpolation;
		SnapshotConfig snapshot;
		SimulationConfig simulation;
		DiagnosticsConfig diagnostics;
	};
}