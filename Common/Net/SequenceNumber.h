#pragma once

#include <cstdint>

namespace common::net
{
	using SequenceNumber = std::uint32_t;

	// 32비트 시퀀스 번호의 순환시 앞뒤 관계 판단 기준값
	inline constexpr SequenceNumber sequenceNumberHalfRange = static_cast<SequenceNumber>(1) << 31;

	[[nodiscard]] inline bool IsSequenceNewer(SequenceNumber lhs, SequenceNumber rhs) noexcept
	{
		if (lhs == rhs)
		{
			return false;
		}

		if (lhs > rhs)
		{
			return (lhs - rhs) < sequenceNumberHalfRange;
		}

		return (rhs - lhs) > sequenceNumberHalfRange;
	}

	[[nodiscard]] inline bool IsSequenceOlder(SequenceNumber lhs, SequenceNumber rhs) noexcept
	{
		return IsSequenceNewer(rhs, lhs);
	}

	[[nodiscard]] inline bool IsSequenceOlderOrEqual(SequenceNumber lhs, SequenceNumber rhs) noexcept
	{
		return lhs == rhs || IsSequenceOlder(lhs, rhs);
	}
}