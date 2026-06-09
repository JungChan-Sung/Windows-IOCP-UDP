#pragma once

#include <cstdint>

namespace server::diagnostics
{
	struct ServerMetricsSnapshot
	{
	public:
		std::uint64_t receivedPacketCount = 0;
		std::uint64_t invalidPacketDropCount = 0;

		// Counts packets that passed dispatcher/header/payload validation and reached UdpServer handlers.
		std::uint64_t joinRequestCount = 0;
		std::uint64_t inputCommandCount = 0;
		std::uint64_t fireRequestCount = 0;
		std::uint64_t leaveRequestCount = 0;
		std::uint64_t joinRoomRequestCount = 0;

		// Counts successful async send request submissions. Actual send completion is handled by IOCP.
		std::uint64_t playerSnapshotSendRequestCount = 0;
		std::uint64_t bulletSnapshotSendRequestCount = 0;
		std::uint64_t impactEffectSendRequestCount = 0;

		std::uint64_t reliableResendPacketCount = 0;
		std::uint64_t reliableResendGiveUpPacketCount = 0;
		std::uint64_t reliableSendWindowFullCount = 0;
		std::uint64_t reliableDataReceivePacketCount = 0;
		std::uint64_t reliableDataSendPacketCount = 0;
		std::uint64_t reliableAckOnlyReceivePacketCount = 0;
		std::uint64_t reliableAckOnlySendPacketCount = 0;
		std::uint64_t reliableDuplicateDropPacketCount = 0;

		std::uint64_t timedOutPeerCount = 0;
	};
}