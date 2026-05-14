#pragma once

#include <cstdint>

namespace server::diagnostics
{
	struct ServerMetricsSnapshot
	{
	public:
		std::uint64_t receivedPacketCount = 0;
		std::uint64_t invalidPacketDropCount = 0;

		std::uint64_t joinRequestCount = 0;
		std::uint64_t inputCommandCount = 0;
		std::uint64_t fireRequestCount = 0;
		std::uint64_t leaveRequestCount = 0;
		std::uint64_t joinRoomRequestCount = 0;

		std::uint64_t playerSnapshotBroadcastCount = 0;
		std::uint64_t bulletSnapshotBroadcastCount = 0;
		std::uint64_t impactEffectBroadcastCount = 0;

		std::uint64_t timedOutPeerCount = 0;
	};
}