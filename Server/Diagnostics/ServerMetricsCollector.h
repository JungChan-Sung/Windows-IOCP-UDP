#pragma once

#include <atomic>
#include <cstdint>

#include <Server/Diagnostics/ServerMetricsSnapshot.h>

namespace server::diagnostics
{
	class ServerMetricsCollector
	{
	private:
		std::atomic<std::uint64_t> receivedPacketCount_ = 0;
		std::atomic<std::uint64_t> invalidPacketDropCount_ = 0;

		std::atomic<std::uint64_t> joinRequestCount_ = 0;
		std::atomic<std::uint64_t> inputCommandCount_ = 0;
		std::atomic<std::uint64_t> fireRequestCount_ = 0;
		std::atomic<std::uint64_t> leaveRequestCount_ = 0;
		std::atomic<std::uint64_t> joinRoomRequestCount_ = 0;

		std::atomic<std::uint64_t> playerSnapshotBroadcastCount_ = 0;
		std::atomic<std::uint64_t> bulletSnapshotBroadcastCount_ = 0;
		std::atomic<std::uint64_t> impactEffectBroadcastCount_ = 0;

		std::atomic<std::uint64_t> timedOutPeerCount_ = 0;

	public:
		ServerMetricsCollector() = default;
		~ServerMetricsCollector() noexcept = default;

		ServerMetricsCollector(const ServerMetricsCollector&) = delete;
		ServerMetricsCollector& operator=(const ServerMetricsCollector&) = delete;

		ServerMetricsCollector(ServerMetricsCollector&&) = delete;
		ServerMetricsCollector& operator=(ServerMetricsCollector&&) = delete;

	public:
		void Reset() noexcept;

		void IncrementReceivedPacketCount() noexcept;
		void IncrementInvalidPacketDropCount() noexcept;

		void IncrementJoinRequestCount() noexcept;
		void IncrementInputCommandCount() noexcept;
		void IncrementFireRequestCount() noexcept;
		void IncrementLeaveRequestCount() noexcept;
		void IncrementJoinRoomRequestCount() noexcept;

		void AddPlayerSnapshotBroadcastCount(std::uint64_t count) noexcept;
		void AddBulletSnapshotBroadcastCount(std::uint64_t count) noexcept;
		void AddImpactEffectBroadcastCount(std::uint64_t count) noexcept;

		void AddTimedOutPeerCount(std::uint64_t count) noexcept;

		[[nodiscard]] ServerMetricsSnapshot CaptureSnapshot() const noexcept;
	};
}