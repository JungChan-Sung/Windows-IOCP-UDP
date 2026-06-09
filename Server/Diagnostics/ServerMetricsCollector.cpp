#include "ServerMetricsCollector.h"

namespace server::diagnostics
{
	void ServerMetricsCollector::Reset() noexcept
	{
		receivedPacketCount_.store(0);
		invalidPacketDropCount_.store(0);

		joinRequestCount_.store(0);
		inputCommandCount_.store(0);
		fireRequestCount_.store(0);
		leaveRequestCount_.store(0);
		joinRoomRequestCount_.store(0);

		playerSnapshotSendRequestCount_.store(0);
		bulletSnapshotSendRequestCount_.store(0);
		impactEffectSendRequestCount_.store(0);

		reliableResendPacketCount_.store(0);
		reliableAckOnlyReceivePacketCount_.store(0);
		reliableAckOnlySendPacketCount_.store(0);
		reliableDuplicateDropPacketCount_.store(0);

		timedOutPeerCount_.store(0);
	}

	void ServerMetricsCollector::IncrementReceivedPacketCount() noexcept
	{
		++receivedPacketCount_;
	}

	void ServerMetricsCollector::IncrementInvalidPacketDropCount() noexcept
	{
		++invalidPacketDropCount_;
	}

	void ServerMetricsCollector::IncrementJoinRequestCount() noexcept
	{
		++joinRequestCount_;
	}

	void ServerMetricsCollector::IncrementInputCommandCount() noexcept
	{
		++inputCommandCount_;
	}

	void ServerMetricsCollector::IncrementFireRequestCount() noexcept
	{
		++fireRequestCount_;
	}

	void ServerMetricsCollector::IncrementLeaveRequestCount() noexcept
	{
		++leaveRequestCount_;
	}

	void ServerMetricsCollector::IncrementJoinRoomRequestCount() noexcept
	{
		++joinRoomRequestCount_;
	}

	void ServerMetricsCollector::AddPlayerSnapshotSendRequestCount(std::uint64_t count) noexcept
	{
		playerSnapshotSendRequestCount_.fetch_add(count);
	}

	void ServerMetricsCollector::AddBulletSnapshotSendRequestCount(std::uint64_t count) noexcept
	{
		bulletSnapshotSendRequestCount_.fetch_add(count);
	}

	void ServerMetricsCollector::AddImpactEffectSendRequestCount(std::uint64_t count) noexcept
	{
		impactEffectSendRequestCount_.fetch_add(count);
	}

	void ServerMetricsCollector::AddReliableResendPacketCount(std::uint64_t count) noexcept
	{
		reliableResendPacketCount_.fetch_add(count);
	}

	void ServerMetricsCollector::IncrementReliableAckOnlyReceivePacketCount() noexcept
	{
		++reliableAckOnlyReceivePacketCount_;
	}

	void ServerMetricsCollector::IncrementReliableAckOnlySendPacketCount() noexcept
	{
		++reliableAckOnlySendPacketCount_;
	}

	void ServerMetricsCollector::IncrementReliableDuplicateDropPacketCount() noexcept
	{
		++reliableDuplicateDropPacketCount_;
	}

	void ServerMetricsCollector::AddTimedOutPeerCount(std::uint64_t count) noexcept
	{
		timedOutPeerCount_.fetch_add(count);
	}

	ServerMetricsSnapshot ServerMetricsCollector::CaptureSnapshot() const noexcept
	{
		ServerMetricsSnapshot snapshot{};

		snapshot.receivedPacketCount = receivedPacketCount_.load();
		snapshot.invalidPacketDropCount = invalidPacketDropCount_.load();

		snapshot.joinRequestCount = joinRequestCount_.load();
		snapshot.inputCommandCount = inputCommandCount_.load();
		snapshot.fireRequestCount = fireRequestCount_.load();
		snapshot.leaveRequestCount = leaveRequestCount_.load();
		snapshot.joinRoomRequestCount = joinRoomRequestCount_.load();

		snapshot.playerSnapshotSendRequestCount = playerSnapshotSendRequestCount_.load();
		snapshot.bulletSnapshotSendRequestCount = bulletSnapshotSendRequestCount_.load();
		snapshot.impactEffectSendRequestCount = impactEffectSendRequestCount_.load();

		snapshot.reliableResendPacketCount = reliableResendPacketCount_.load();
		snapshot.reliableAckOnlyReceivePacketCount = reliableAckOnlyReceivePacketCount_.load();
		snapshot.reliableAckOnlySendPacketCount = reliableAckOnlySendPacketCount_.load();
		snapshot.reliableDuplicateDropPacketCount = reliableDuplicateDropPacketCount_.load();

		snapshot.timedOutPeerCount = timedOutPeerCount_.load();

		return snapshot;
	}
}