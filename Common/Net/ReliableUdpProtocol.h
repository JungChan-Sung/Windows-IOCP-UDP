#pragma once

#include <cstdint>
#include <limits>

namespace common::net
{
	using ReliableSequence = std::uint32_t;

	inline constexpr int reliableAckBitCount = 32;
	inline constexpr ReliableSequence reliableSequenceHalfRange = static_cast<ReliableSequence>(1) << 31;

	[[nodiscard]] inline bool IsSequenceNewer(ReliableSequence lhs, ReliableSequence rhs) noexcept
	{
		if (lhs == rhs)
		{
			return false;
		}

		if (lhs > rhs)
		{
			return (lhs - rhs) < reliableSequenceHalfRange;
		}

		return (rhs - lhs) > reliableSequenceHalfRange;
	}

	[[nodiscard]] inline bool IsSequenceOlder(ReliableSequence lhs, ReliableSequence rhs) noexcept
	{
		return IsSequenceNewer(rhs, lhs);
	}

	[[nodiscard]] inline bool IsSequenceAcked(ReliableSequence sequence, ReliableSequence ackSequence, std::uint32_t ackBitfield) noexcept
	{
		if (sequence == ackSequence)
		{
			return true;
		}

		if (IsSequenceNewer(sequence, ackSequence))
		{
			return false;
		}

		const ReliableSequence distance = ackSequence - sequence;
		if (distance == 0 || distance > reliableAckBitCount)
		{
			return false;
		}

		const std::uint32_t ackBit = static_cast<std::uint32_t>(1) << (distance - 1);
		return (ackBitfield & ackBit) != 0;
	}

	class ReliableAckTracker
	{
	private:
		ReliableSequence latestReceivedSequence_ = 0;
		std::uint32_t ackBitfield_ = 0;
		bool hasReceivedAnySequence_ = false;

	public:
		ReliableAckTracker() = default;
		~ReliableAckTracker() noexcept = default;

		ReliableAckTracker(const ReliableAckTracker&) = default;
		ReliableAckTracker& operator=(const ReliableAckTracker&) = default;

		ReliableAckTracker(ReliableAckTracker&&) noexcept = default;
		ReliableAckTracker& operator=(ReliableAckTracker&&) noexcept = default;

	public:
		void Reset() noexcept
		{
			latestReceivedSequence_ = 0;
			ackBitfield_ = 0;
			hasReceivedAnySequence_ = false;
		}

		void ObserveReceivedSequence(ReliableSequence sequence) noexcept
		{
			if (!hasReceivedAnySequence_)
			{
				latestReceivedSequence_ = sequence;
				ackBitfield_ = 0;
				hasReceivedAnySequence_ = true;
				return;
			}

			if (sequence == latestReceivedSequence_)
			{
				return;
			}

			if (IsSequenceNewer(sequence, latestReceivedSequence_))
			{
				const ReliableSequence distance = sequence - latestReceivedSequence_;

				if (distance > reliableAckBitCount)
				{
					ackBitfield_ = 0;
				}
				else
				{
					ackBitfield_ <<= distance;
					ackBitfield_ |= static_cast<std::uint32_t>(1) << (distance - 1);
				}

				latestReceivedSequence_ = sequence;
				return;
			}

			const ReliableSequence distance = latestReceivedSequence_ - sequence;
			if (distance == 0 || distance > reliableAckBitCount)
			{
				return;
			}

			ackBitfield_ |= static_cast<std::uint32_t>(1) << (distance - 1);
		}

	public:
		[[nodiscard]] bool IsSequenceAcked(ReliableSequence sequence) const noexcept
		{
			if (!hasReceivedAnySequence_)
			{
				return false;
			}

			return net::IsSequenceAcked(sequence, latestReceivedSequence_, ackBitfield_);
		}

		[[nodiscard]] bool HasReceivedAnySequence() const noexcept
		{
			return hasReceivedAnySequence_;
		}

		[[nodiscard]] ReliableSequence GetLatestReceivedSequence() const noexcept
		{
			return latestReceivedSequence_;
		}

		[[nodiscard]] std::uint32_t GetAckBitfield() const noexcept
		{
			return ackBitfield_;
		}
	};
}