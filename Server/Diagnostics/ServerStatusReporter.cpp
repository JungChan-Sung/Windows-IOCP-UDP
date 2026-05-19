#include "ServerStatusReporter.h"

#include <sstream>

namespace server::diagnostics
{
	void ServerStatusReporter::Reset() noexcept
	{
		nextReportTime_ = TimePoint();
		isFirstCheck_ = true;
	}

	bool ServerStatusReporter::ShouldReport(TimePoint currentTime) noexcept
	{
		if (!isEnabled_)
		{
			return false;
		}

		if (reportInterval_ <= Duration::zero())
		{
			return false;
		}

		if (isFirstCheck_)
		{
			isFirstCheck_ = false;
			nextReportTime_ = currentTime + reportInterval_;
			return false;
		}

		if (currentTime < nextReportTime_)
		{
			return false;
		}

		nextReportTime_ = currentTime + reportInterval_;
		return true;
	}

	std::string ServerStatusReporter::BuildMessage(const ServerStatusSnapshot& snapshot) const
	{
		std::ostringstream stream;
		stream << "Server status. "
			<< "Tick=" << snapshot.serverTick
			<< ", Peers=" << snapshot.peerCount
			<< ", JoinedPeers=" << snapshot.joinedPeerCount
			<< ", Rooms=" << snapshot.roomCount
			<< ", Players=" << snapshot.playerCount
			<< ", Bullets=" << snapshot.bulletCount
			<< ", PendingEffects=" << snapshot.pendingImpactEffectCount
			<< ", PendingSendContexts=" << snapshot.pendingSendContextCount
			<< ", UdpSendCompletions=" << snapshot.udpSendCompletionCount
			<< ", UdpSendCompletionFailures=" << snapshot.udpSendCompletionFailureCount
			<< ", UdpSendCompletedBytes=" << snapshot.udpSendCompletedByteCount
			<< ", ReceivedPackets=" << snapshot.metrics.receivedPacketCount
			<< ", InvalidPacketDrops=" << snapshot.metrics.invalidPacketDropCount
			<< ", JoinRequests=" << snapshot.metrics.joinRequestCount
			<< ", InputCommands=" << snapshot.metrics.inputCommandCount
			<< ", FireRequests=" << snapshot.metrics.fireRequestCount
			<< ", LeaveRequests=" << snapshot.metrics.leaveRequestCount
			<< ", JoinRoomRequests=" << snapshot.metrics.joinRoomRequestCount
			<< ", PlayerSnapshotSendRequestCount=" << snapshot.metrics.playerSnapshotSendRequestCount
			<< ", BulletSnapshotSendRequestCount=" << snapshot.metrics.bulletSnapshotSendRequestCount
			<< ", ImpactEffectSendRequestCount=" << snapshot.metrics.impactEffectSendRequestCount
			<< ", TimedOutPeers=" << snapshot.metrics.timedOutPeerCount;

		return stream.str();
	}
}