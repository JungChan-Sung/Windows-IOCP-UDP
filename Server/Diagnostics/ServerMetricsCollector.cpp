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

		playerSnapshotBroadcastCount_.store(0);
		bulletSnapshotBroadcastCount_.store(0);
		impactEffectBroadcastCount_.store(0);

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

	void ServerMetricsCollector::AddPlayerSnapshotBroadcastCount(std::uint64_t count) noexcept
	{
		playerSnapshotBroadcastCount_.fetch_add(count);
	}

	void ServerMetricsCollector::AddBulletSnapshotBroadcastCount(std::uint64_t count) noexcept
	{
		bulletSnapshotBroadcastCount_.fetch_add(count);
	}

	void ServerMetricsCollector::AddImpactEffectBroadcastCount(std::uint64_t count) noexcept
	{
		impactEffectBroadcastCount_.fetch_add(count);
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

		snapshot.playerSnapshotBroadcastCount = playerSnapshotBroadcastCount_.load();
		snapshot.bulletSnapshotBroadcastCount = bulletSnapshotBroadcastCount_.load();
		snapshot.impactEffectBroadcastCount = impactEffectBroadcastCount_.load();

		snapshot.timedOutPeerCount = timedOutPeerCount_.load();

		return snapshot;
	}
}