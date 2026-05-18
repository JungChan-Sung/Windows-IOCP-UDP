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

		playerSnapshotSendCount_.store(0);
		bulletSnapshotSendCount_.store(0);
		impactEffectSendCount_.store(0);

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

		snapshot.timedOutPeerCount = timedOutPeerCount_.load();

		return snapshot;
	}
}