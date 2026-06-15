#pragma once

#include <cstddef>
#include <cstdint>

#include <Server/Diagnostics/ServerMetricsSnapshot.h>

namespace server::diagnostics
{
	struct ServerStatusSnapshot
	{
	public:
		std::uint32_t serverTick = 0;

		std::size_t peerCount = 0;
		std::size_t joinedPeerCount = 0;
		std::size_t roomCount = 0;

		std::size_t playerCount = 0;
		std::size_t bulletCount = 0;
		std::size_t pendingImpactEffectCount = 0;

		std::size_t pendingSendContextCount = 0;
		std::size_t reliablePendingPacketCount = 0;
		std::size_t faultSimulationPendingPacketCount = 0;

		std::uint64_t udpSendCompletionCount = 0;
		std::uint64_t udpSendCompletionFailureCount = 0;
		std::uint64_t udpSendCompletedByteCount = 0;

		ServerMetricsSnapshot metrics{};
	};
}