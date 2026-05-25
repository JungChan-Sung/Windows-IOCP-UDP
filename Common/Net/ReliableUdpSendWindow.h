#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

#include <Common/Net/ReliableUdpProtocol.h>

namespace common::net
{
	struct ReliablePendingPacket
	{
	public:
		ReliableSequence sequence = 0;
		std::vector<char> packetBuffer;
		std::chrono::steady_clock::time_point lastSentTime;
		int resendCount = 0;
	};

	class ReliableUdpSendWindow
	{
	public:
		using Clock = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;
		using Duration = Clock::duration;
		using PendingPacketList = std::deque<ReliablePendingPacket>;
		using ResendPacketList = std::vector<ReliablePendingPacket>;

	private:
		ReliableSequence nextSequence_ = 1;
		PendingPacketList pendingPacketList_;
		std::size_t maxPendingPacketCount_ = 64;
		Duration resendInterval_ = std::chrono::milliseconds(100);

	public:
		ReliableUdpSendWindow() = default;
		~ReliableUdpSendWindow() noexcept = default;

		ReliableUdpSendWindow(const ReliableUdpSendWindow&) = delete;
		ReliableUdpSendWindow& operator=(const ReliableUdpSendWindow&) = delete;

		ReliableUdpSendWindow(ReliableUdpSendWindow&&) noexcept = default;
		ReliableUdpSendWindow& operator=(ReliableUdpSendWindow&&) noexcept = default;

	public:
		void Reset() noexcept
		{
			nextSequence_ = 1;
			pendingPacketList_.clear();
		}

		[[nodiscard]] std::optional<ReliableSequence> RegisterSentPacket(std::vector<char> packetBuffer, TimePoint sentTime)
		{
			if (packetBuffer.empty())
			{
				return std::nullopt;
			}

			if (pendingPacketList_.size() >= maxPendingPacketCount_)
			{
				return std::nullopt;
			}

			const ReliableSequence sequence = nextSequence_;
			++nextSequence_;

			ReliablePendingPacket pendingPacket{};
			pendingPacket.sequence = sequence;
			pendingPacket.packetBuffer = std::move(packetBuffer);
			pendingPacket.lastSentTime = sentTime;
			pendingPacket.resendCount = 0;

			pendingPacketList_.push_back(std::move(pendingPacket));
			return sequence;
		}

		void ProcessAck(ReliableSequence ackSequence, std::uint32_t ackBitfield)
		{
			for (auto packetIterator = pendingPacketList_.begin(); packetIterator != pendingPacketList_.end();)
			{
				if (IsSequenceAcked(packetIterator->sequence, ackSequence, ackBitfield))
				{
					packetIterator = pendingPacketList_.erase(packetIterator);
					continue;
				}

				++packetIterator;
			}
		}

		[[nodiscard]] ResendPacketList ExtractResendPackets(TimePoint currentTime)
		{
			ResendPacketList resendPacketList;
			for (ReliablePendingPacket& pendingPacket : pendingPacketList_)
			{
				if (currentTime - pendingPacket.lastSentTime < resendInterval_)
				{
					continue;
				}

				pendingPacket.lastSentTime = currentTime;
				++pendingPacket.resendCount;
				resendPacketList.push_back(pendingPacket);
			}

			return resendPacketList;
		}

	public:
		void SetMaxPendingPacketCount(std::size_t maxPendingPacketCount) noexcept
		{
			maxPendingPacketCount_ = maxPendingPacketCount;
		}

		void SetResendInterval(Duration resendInterval) noexcept
		{
			resendInterval_ = resendInterval;
		}

		[[nodiscard]] ReliableSequence GetNextSequence() const noexcept
		{
			return nextSequence_;
		}

		[[nodiscard]] std::size_t GetPendingPacketCount() const noexcept
		{
			return pendingPacketList_.size();
		}

		[[nodiscard]] std::size_t GetMaxPendingPacketCount() const noexcept
		{
			return maxPendingPacketCount_;
		}

		[[nodiscard]] Duration GetResendInterval() const noexcept
		{
			return resendInterval_;
		}
	};
}