#include "PacketReplayGuard.h"

#include <cstdint>

namespace common::net
{
	void PacketReplayGuard::Reset() noexcept
	{
		latestSequence_ = 0;
		receivedBitfield_ = 0;

		hasReceivedSequence_ = false;
	}

	bool PacketReplayGuard::TryAccept(SequenceNumber sequence) noexcept
	{
		if (!hasReceivedSequence_)
		{
			latestSequence_ = sequence;
			receivedBitfield_ = 1;
			hasReceivedSequence_ = true;

			return true;
		}

		if (sequence == latestSequence_)
		{
			return false;
		}

		if (IsSequenceNewer(sequence, latestSequence_))
		{
			const SequenceNumber distance = sequence - latestSequence_;
			if (distance >= replayWindowBitCount)
			{
				receivedBitfield_ = 1;
			}
			else
			{
				receivedBitfield_ <<= distance;
				receivedBitfield_ |= 1;
			}

			latestSequence_ = sequence;
			return true;
		}

		const SequenceNumber distance = latestSequence_ - sequence;
		if (distance >= replayWindowBitCount)
		{
			return false;
		}

		const std::uint64_t sequenceBit = static_cast<std::uint64_t>(1) << distance;
		if ((receivedBitfield_ & sequenceBit) != 0)
		{
			return false;
		}

		receivedBitfield_ |= sequenceBit;
		return true;
	}
}