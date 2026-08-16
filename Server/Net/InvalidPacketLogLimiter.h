#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

#include <Common/Time/TimeTypes.h>

#include <Server/Protocol/UdpPacketDispatcher.h>

namespace server::net
{
	class InvalidPacketLogLimiter
	{
	public:
		using DispatchStatus = protocol::UdpPacketDispatcher::DispatchStatus;
		using Clock = common::time::Clock;
		using TimePoint = common::time::TimePoint;
		using Duration = common::time::Duration;

	public:
		struct LogDecision
		{
		public:
			bool shouldLog = false;
			std::uint64_t totalCount = 0;
			std::uint64_t suppressedCount = 0;
		};

	private:
		struct StatusState
		{
		public:
			std::uint64_t totalCount = 0;
			std::uint64_t suppressedCount = 0;
			TimePoint nextLogTime;
		};

	private:
		static inline constexpr std::uint64_t immediateLogCount = 3;
		static inline constexpr Duration logInterval = common::time::Seconds(5);
		static inline constexpr std::size_t statusCount = static_cast<std::size_t>(DispatchStatus::Count);

	private:
		mutable std::mutex mutex_;
		std::array<StatusState, statusCount> statusStateList_{};

	public:
		InvalidPacketLogLimiter() = default;
		~InvalidPacketLogLimiter() noexcept = default;

		InvalidPacketLogLimiter(const InvalidPacketLogLimiter&) = delete;
		InvalidPacketLogLimiter& operator=(const InvalidPacketLogLimiter&) = delete;

		InvalidPacketLogLimiter(InvalidPacketLogLimiter&&) = delete;
		InvalidPacketLogLimiter& operator=(InvalidPacketLogLimiter&&) = delete;

	public:
		[[nodiscard]] LogDecision Record(DispatchStatus status, TimePoint currentTime);
		void Reset() noexcept;

	public:
		[[nodiscard]] std::uint64_t GetTotalDroppedCount() const;

	private:
		[[nodiscard]] static std::size_t GetStatusIndex(DispatchStatus status) noexcept;
	};
}