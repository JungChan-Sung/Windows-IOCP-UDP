#include "PacketReplayGuard.h"

#include <cstddef>

namespace common::net
{
	void PacketReplayGuard::Reset() noexcept
	{
		latestSequence_ = 0;
		receivedBitfield_.reset();

		hasReceivedSequence_ = false;
	}

	bool PacketReplayGuard::TryAccept(SequenceNumber sequence) noexcept
	{
		if (!hasReceivedSequence_)
		{
			latestSequence_ = sequence;
			receivedBitfield_.set(0);
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
				receivedBitfield_.reset();
			}
			else
			{
				receivedBitfield_ <<= static_cast<std::size_t>(distance);
			}

			receivedBitfield_.set(0);
			latestSequence_ = sequence;

			return true;
		}

		const SequenceNumber distance = latestSequence_ - sequence;
		if (distance >= replayWindowBitCount)
		{
			return false;
		}

		const std::size_t bitIndex = static_cast<std::size_t>(distance);
		if (receivedBitfield_.test(bitIndex))
		{
			return false;
		}

		receivedBitfield_.set(bitIndex);

		return true;
	}
}