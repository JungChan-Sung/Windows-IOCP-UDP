#pragma once

#include <cstddef>
#include <cstdint>

#include <Common/Net/SequenceNumber.h>

namespace common::net
{
	class PacketReplayGuard
	{
	private:
		static inline constexpr std::size_t replayWindowBitCount = 64;

	private:
		SequenceNumber latestSequence_ = 0;
		std::uint64_t receivedBitfield_ = 0;

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

		[[nodiscard]] bool TryAccept(SequenceNumber sequence) noexcept;
	};
}