#pragma once

#include <bitset>
#include <cstddef>

#include <Common/Net/SequenceNumber.h>

namespace common::net
{
	class PacketReplayGuard
	{
	public:
		enum class ObserveStatus
		{
			New,
			Duplicate,
			TooOld,
		};

	private:
		static inline constexpr std::size_t replayWindowBitCount = 256;

	private:
		SequenceNumber latestSequence_ = 0;

		std::bitset<replayWindowBitCount> receivedBitfield_;

		bool hasReceivedSequence_ = false;

	public:
		PacketReplayGuard() = default;
		~PacketReplayGuard() noexcept = default;

		PacketReplayGuard(const PacketReplayGuard&) = default;
		PacketReplayGuard& operator=(const PacketReplayGuard&) = default;

		PacketReplayGuard(PacketReplayGuard&&) noexcept = default;
		PacketReplayGuard& operator=(PacketReplayGuard&&) noexcept = default;

	public:
		void Reset() noexcept;

		[[nodiscard]] ObserveStatus Observe(SequenceNumber sequence) noexcept;

		[[nodiscard]] bool TryAccept(SequenceNumber sequence) noexcept;
	};
}