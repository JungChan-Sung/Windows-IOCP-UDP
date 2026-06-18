#include "ClientConfigValidator.h"

#include <chrono>
#include <limits>
#include <utility>

#include <Common/Time/TimeTypes.h>

namespace
{
	void AddWarning(std::vector<client::config::ClientConfigWarning>& warningList, std::string message)
	{
		client::config::ClientConfigWarning warning{};
		warning.lineNumber = 0;
		warning.message = std::move(message);

		warningList.push_back(std::move(warning));
	}
}

namespace client::config
{
	std::vector<ClientConfigWarning> ClientConfigValidator::ValidateAndNormalize(ClientConfig& clientConfig)
	{
		std::vector<ClientConfigWarning> warningList;
		const ClientConfig defaultConfig{};

		if (clientConfig.network.serverIp.empty())
		{
			AddWarning(warningList, "Network.ServerIp cannot be empty. Default server ip will be used.");
			clientConfig.network.serverIp = defaultConfig.network.serverIp;
		}

		if (clientConfig.network.serverPort == 0)
		{
			AddWarning(warningList, "Network.ServerPort cannot be 0. Default server port will be used.");
			clientConfig.network.serverPort = defaultConfig.network.serverPort;
		}

		if (clientConfig.network.iocpWorkerThreadCount == 0)
		{
			AddWarning(
				warningList,
				"Network.IocpWorkerThreadCount must be greater than 0. Default IOCP worker thread count will be used."
			);

			clientConfig.network.iocpWorkerThreadCount = defaultConfig.network.iocpWorkerThreadCount;
		}

		if (clientConfig.network.iocpRecvContextCount == 0)
		{
			AddWarning(
				warningList,
				"Network.IocpRecvContextCount must be greater than 0. Default IOCP recv context count will be used."
			);

			clientConfig.network.iocpRecvContextCount = defaultConfig.network.iocpRecvContextCount;
		}

		if (clientConfig.timing.updateSleepInterval <= common::time::Milliseconds(0))
		{
			AddWarning(warningList, "Timing.UpdateSleepMs must be greater than 0. Default update sleep will be used.");
			clientConfig.timing.updateSleepInterval = defaultConfig.timing.updateSleepInterval;
		}

		if (clientConfig.timing.joinRetryInterval <= common::time::Milliseconds(0))
		{
			AddWarning(warningList, "Timing.JoinRetryMs must be greater than 0. Default join retry interval will be used.");
			clientConfig.timing.joinRetryInterval = defaultConfig.timing.joinRetryInterval;
		}

		if (clientConfig.timing.roomJoinInterval <= common::time::Milliseconds(0))
		{
			AddWarning(warningList, "Timing.RoomJoinMs must be greater than 0. Default room join interval will be used.");
			clientConfig.timing.roomJoinInterval = defaultConfig.timing.roomJoinInterval;
		}

		if (clientConfig.timing.interpolationAdjustStep <= common::time::Milliseconds(0))
		{
			AddWarning(
				warningList,
				"Timing.InterpolationAdjustStepMs must be greater than 0. Default interpolation adjust step will be used."
			);
			clientConfig.timing.interpolationAdjustStep = defaultConfig.timing.interpolationAdjustStep;
		}

		if (clientConfig.interpolation.minDelay > clientConfig.interpolation.maxDelay)
		{
			AddWarning(
				warningList,
				"Interpolation.MinDelayMs cannot be greater than Interpolation.MaxDelayMs. Default interpolation range will be used."
			);

			clientConfig.interpolation.minDelay = defaultConfig.interpolation.minDelay;
			clientConfig.interpolation.maxDelay = defaultConfig.interpolation.maxDelay;
		}

		if (clientConfig.interpolation.defaultDelay < clientConfig.interpolation.minDelay)
		{
			AddWarning(warningList, "Interpolation.DefaultDelayMs is lower than MinDelayMs. It will be clamped to MinDelayMs.");
			clientConfig.interpolation.defaultDelay = clientConfig.interpolation.minDelay;
		}

		if (clientConfig.interpolation.defaultDelay > clientConfig.interpolation.maxDelay)
		{
			AddWarning(warningList, "Interpolation.DefaultDelayMs is greater than MaxDelayMs. It will be clamped to MaxDelayMs.");
			clientConfig.interpolation.defaultDelay = clientConfig.interpolation.maxDelay;
		}

		if (clientConfig.snapshot.assemblyTimeout <= common::time::Milliseconds(0))
		{
			AddWarning(warningList, "Snapshot.AssemblyTimeoutMs must be greater than 0. Default timeout will be used.");
			clientConfig.snapshot.assemblyTimeout = defaultConfig.snapshot.assemblyTimeout;
		}

		if (clientConfig.simulation.tickInterval <= common::time::Milliseconds(0))
		{
			AddWarning(warningList, "Simulation.TickIntervalMs must be greater than 0. Default tick interval will be used.");
			clientConfig.simulation.tickInterval = defaultConfig.simulation.tickInterval;
		}

		if (clientConfig.simulation.deltaSeconds <= 0.0F)
		{
			AddWarning(warningList, "Simulation.DeltaSeconds must be greater than 0. Default delta seconds will be used.");
			clientConfig.simulation.deltaSeconds = defaultConfig.simulation.deltaSeconds;
		}

		return warningList;
	}
}