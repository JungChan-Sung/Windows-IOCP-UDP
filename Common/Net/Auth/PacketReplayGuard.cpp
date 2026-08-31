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

	PacketReplayGuard::ObserveStatus PacketReplayGuard::Observe(SequenceNumber sequence) noexcept
	{
		if (!hasReceivedSequence_)
		{
			latestSequence_ = sequence;
			receivedBitfield_.set(0);
			hasReceivedSequence_ = true;

			return ObserveStatus::New;
		}

		if (sequence == latestSequence_)
		{
			return ObserveStatus::Duplicate;
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

			return ObserveStatus::New;
		}

		const SequenceNumber distance = latestSequence_ - sequence;
		if (distance >= replayWindowBitCount)
		{
			return ObserveStatus::TooOld;
		}

		const std::size_t bitIndex = static_cast<std::size_t>(distance);
		if (receivedBitfield_.test(bitIndex))
		{
			return ObserveStatus::Duplicate;
		}

		receivedBitfield_.set(bitIndex);

		return ObserveStatus::New;
	}

	bool PacketReplayGuard::TryAccept(SequenceNumber sequence) noexcept
	{
		return Observe(sequence) == ObserveStatus::New;
	}
}