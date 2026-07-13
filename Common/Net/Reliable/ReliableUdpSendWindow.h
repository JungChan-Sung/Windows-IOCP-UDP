#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include <Common/Net/ReliableUdpProtocol.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Time/TimeTypes.h>

namespace common::net
{
	struct ReliablePendingPacket
	{
	public:
		ReliableSequence sequence = 0;
		common::packet::PacketBuffer packetBuffer;
		common::time::TimePoint lastSentTime;
		int resendCount = 0;
	};

	struct ReliableResendResult
	{
	public:
		std::vector<ReliablePendingPacket> resendPacketList;
		std::vector<ReliablePendingPacket> giveUpPacketList;
	};

	class ReliableUdpSendWindow
	{
	public:
		using Clock = common::time::Clock;
		using TimePoint = common::time::TimePoint;
		using Duration = common::time::Duration;
		using PendingPacketList = std::deque<ReliablePendingPacket>;
		using ResendPacketList = std::vector<ReliablePendingPacket>;
		using ResendResult = ReliableResendResult;

	private:
		ReliableSequence nextSequence_ = 1;
		PendingPacketList pendingPacketList_;
		std::size_t maxPendingPacketCount_ = 64;
		int maxResendCount_ = 10;
		Duration resendInterval_;

	public:
		ReliableUdpSendWindow()
			: resendInterval_(common::time::Milliseconds(100))
		{}
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

		[[nodiscard]] bool CanRegisterSentPacket(const common::packet::PacketBuffer& packetBuffer) const noexcept
		{
			return !packetBuffer.empty() && pendingPacketList_.size() < maxPendingPacketCount_;
		}

		[[nodiscard]] ReliableSequence AllocateSequence() noexcept
		{
			const ReliableSequence sequence = nextSequence_;
			++nextSequence_;

			return sequence;
		}

		[[nodiscard]] bool RegisterSentPacket(ReliableSequence sequence, common::packet::PacketBuffer packetBuffer, TimePoint sentTime)
		{
			if (!CanRegisterSentPacket(packetBuffer))
			{
				return false;
			}

			ReliablePendingPacket pendingPacket{};
			pendingPacket.sequence = sequence;
			pendingPacket.packetBuffer = std::move(packetBuffer);
			pendingPacket.lastSentTime = sentTime;
			pendingPacket.resendCount = 0;

			pendingPacketList_.push_back(std::move(pendingPacket));
			return true;
		}

		[[nodiscard]] std::optional<ReliableSequence> RegisterSentPacket(common::packet::PacketBuffer packetBuffer, TimePoint sentTime)
		{
			if (!CanRegisterSentPacket(packetBuffer))
			{
				return std::nullopt;
			}

			const ReliableSequence sequence = AllocateSequence();
			const bool registerResult = RegisterSentPacket(sequence, std::move(packetBuffer), sentTime);

			if (!registerResult)
			{
				return std::nullopt;
			}

			return sequence;
		}

		[[nodiscard]] bool ProcessAck(ReliableSequence ackSequence, std::uint32_t ackBitfield)
		{
			if (!CanProcessAck(ackSequence))
			{
				return false;
			}

			for (auto packetIterator = pendingPacketList_.begin(); packetIterator != pendingPacketList_.end();)
			{
				if (IsSequenceAcked(packetIterator->sequence, ackSequence, ackBitfield))
				{
					packetIterator = pendingPacketList_.erase(packetIterator);
					continue;
				}

				++packetIterator;
			}

			return true;
		}

		[[nodiscard]] bool CanProcessAck(ReliableSequence ackSequence) const noexcept
		{
			if (ackSequence == 0)
			{
				return true;
			}

			const ReliableSequence latestAllocatedSequence = nextSequence_ - 1;
			return !IsSequenceNewer(ackSequence, latestAllocatedSequence);
		}

		[[nodiscard]] ResendResult ExtractResendResult(TimePoint currentTime)
		{
			ResendResult result{};

			for (auto packetIterator = pendingPacketList_.begin(); packetIterator != pendingPacketList_.end();)
			{
				ReliablePendingPacket& pendingPacket = *packetIterator;
				if (currentTime - pendingPacket.lastSentTime < resendInterval_)
				{
					++packetIterator;
					continue;
				}

				if (pendingPacket.resendCount >= maxResendCount_)
				{
					result.giveUpPacketList.push_back(std::move(pendingPacket));
					packetIterator = pendingPacketList_.erase(packetIterator);
					continue;
				}

				pendingPacket.lastSentTime = currentTime;
				++pendingPacket.resendCount;
				result.resendPacketList.push_back(pendingPacket);

				++packetIterator;
			}

			return result;
		}

		[[nodiscard]] ResendPacketList ExtractResendPackets(TimePoint currentTime)
		{
			ResendResult result = ExtractResendResult(currentTime);
			return std::move(result.resendPacketList);
		}

	public:
		void SetMaxPendingPacketCount(std::size_t maxPendingPacketCount) noexcept
		{
			maxPendingPacketCount_ = maxPendingPacketCount;
		}

		void SetMaxResendCount(int maxResendCount) noexcept
		{
			maxResendCount_ = (maxResendCount < 0) ? 0 : maxResendCount;
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

		[[nodiscard]] int GetMaxResendCount() const noexcept
		{
			return maxResendCount_;
		}

		[[nodiscard]] Duration GetResendInterval() const noexcept
		{
			return resendInterval_;
		}
	};
}