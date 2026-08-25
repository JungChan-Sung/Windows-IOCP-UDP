#pragma once

#include <cstddef>
#include <string>

#include <Client/Config/ClientTransportType.h>
#include <Client/Config/ClientConfigDefaults.h>
#include <Common/Time/TimeTypes.h>

namespace client::config
{
	struct NetworkConfig
	{
	public:
		std::string serverIp = std::string(defaultServerIp);
		unsigned short serverPort = defaultServerPort;
		ClientTransportType transportType = ClientTransportType::Socket;
		std::size_t iocpWorkerThreadCount = defaultIocpWorkerThreadCount;
		std::size_t iocpRecvContextCount = defaultIocpRecvContextCount;
	};

	struct AccountConfig
	{
	public:
		std::string loginName = std::string(defaultAccountLoginName);
		std::string passwordHash = std::string(defaultAccountPasswordHash);
	};

	struct TimingConfig
	{
	public:
		common::time::Milliseconds updateSleepInterval = defaultUpdateSleepInterval;
		common::time::Milliseconds accountLoginRetryInterval = defaultAccountLoginRetryInterval;
		common::time::Milliseconds joinRetryInterval = defaultJoinRetryInterval;
		common::time::Milliseconds keepAliveInterval = defaultKeepAliveInterval;
		common::time::Milliseconds roomJoinInterval = defaultRoomJoinInterval;
		common::time::Milliseconds interpolationAdjustStep = defaultInterpolationAdjustStep;
	};

	struct InterpolationConfig
	{
	public:
		common::time::Milliseconds defaultDelay = defaultInterpolationDelay;
		common::time::Milliseconds minDelay = minInterpolationDelay;
		common::time::Milliseconds maxDelay = maxInterpolationDelay;
	};

	struct SnapshotConfig
	{
	public:
		common::time::Milliseconds assemblyTimeout = defaultSnapshotAssemblyTimeout;
	};

	struct SimulationConfig
	{
	public:
		common::time::Milliseconds tickInterval = defaultSimulationTickInterval;
		float deltaSeconds = defaultSimulationDeltaSeconds;
	};

	struct DiagnosticsConfig
	{
	public:
		common::log::LogLevel logLevel = defaultLogLevel;
		std::size_t asyncLogWorkerThreadCount = defaultAsyncLogWorkerThreadCount;
	};

	struct ClientConfig
	{
	public:
		NetworkConfig network;
		AccountConfig account;
		TimingConfig timing;
		InterpolationConfig interpolation;
		SnapshotConfig snapshot;
		SimulationConfig simulation;
		DiagnosticsConfig diagnostics;
	};
}