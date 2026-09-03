#pragma once

#include <cstdint>
#include <limits>

#include <Common/Net/SequenceNumber.h>

namespace common::net
{
	using ReliableSequence = std::uint32_t;

	inline constexpr int reliableAckBitCount = 32;

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

	// 가장 최근에 수신한 sequence와 이전 32개 패킷의 수신 여부 추적 클래스
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
					// 최신 sequence가 이동한 만큼 기존 수신 이력의 기준도 함께 이동
					ackBitfield_ <<= distance;
					// 이전 최신 sequence를 새 기준에서 수신된 패킷으로 기록
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

			// 순서가 뒤바뀌어 도착한 패킷은 최신 sequence와의 거리에 해당하는 비트를 기록
			ackBitfield_ |= static_cast<std::uint32_t>(1) << (distance - 1);
		}

	public:
		// 최신 ACK와 비트필드를 사용해 지정한 sequence의 수신 여부 판단 함수
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