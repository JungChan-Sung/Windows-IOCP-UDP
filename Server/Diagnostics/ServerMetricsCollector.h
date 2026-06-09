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

		std::atomic<std::uint64_t> playerSnapshotSendRequestCount_ = 0;
		std::atomic<std::uint64_t> bulletSnapshotSendRequestCount_ = 0;
		std::atomic<std::uint64_t> impactEffectSendRequestCount_ = 0;

		std::atomic<std::uint64_t> reliableResendPacketCount_ = 0;
		std::atomic<std::uint64_t> reliableResendGiveUpPacketCount_ = 0;
		std::atomic<std::uint64_t> reliableDataReceivePacketCount_ = 0;
		std::atomic<std::uint64_t> reliableDataSendPacketCount_ = 0;
		std::atomic<std::uint64_t> reliableAckOnlyReceivePacketCount_ = 0;
		std::atomic<std::uint64_t> reliableAckOnlySendPacketCount_ = 0;
		std::atomic<std::uint64_t> reliableDuplicateDropPacketCount_ = 0;

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

		void AddPlayerSnapshotSendRequestCount(std::uint64_t count) noexcept;
		void AddBulletSnapshotSendRequestCount(std::uint64_t count) noexcept;
		void AddImpactEffectSendRequestCount(std::uint64_t count) noexcept;

		void AddReliableResendPacketCount(std::uint64_t count) noexcept;
		void AddReliableResendGiveUpPacketCount(std::uint64_t count) noexcept;
		void IncrementReliableDataReceivePacketCount() noexcept;
		void IncrementReliableDataSendPacketCount() noexcept;
		void IncrementReliableAckOnlyReceivePacketCount() noexcept;
		void IncrementReliableAckOnlySendPacketCount() noexcept;
		void IncrementReliableDuplicateDropPacketCount() noexcept;

		void AddTimedOutPeerCount(std::uint64_t count) noexcept;

		[[nodiscard]] ServerMetricsSnapshot CaptureSnapshot() const noexcept;
	};
}