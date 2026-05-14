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

	void ServerMetricsCollector::AddPlayerSnapshotSendCount(std::uint64_t count) noexcept
	{
		playerSnapshotSendCount_.fetch_add(count);
	}

	void ServerMetricsCollector::AddBulletSnapshotSendCount(std::uint64_t count) noexcept
	{
		bulletSnapshotSendCount_.fetch_add(count);
	}

	void ServerMetricsCollector::AddImpactEffectSendCount(std::uint64_t count) noexcept
	{
		impactEffectSendCount_.fetch_add(count);
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

		snapshot.playerSnapshotSendCount = playerSnapshotSendCount_.load();
		snapshot.bulletSnapshotSendCount = bulletSnapshotSendCount_.load();
		snapshot.impactEffectSendCount = impactEffectSendCount_.load();

		snapshot.timedOutPeerCount = timedOutPeerCount_.load();

		return snapshot;
	}
}