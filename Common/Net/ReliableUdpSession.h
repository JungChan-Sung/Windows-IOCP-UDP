#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <Common/Net/ReliableUdpPacketHeader.h>
#include <Common/Net/ReliableUdpProtocol.h>
#include <Common/Net/ReliableUdpSendWindow.h>
#include <Common/Packet/PacketBuffer.h>

namespace common::net
{
	class ReliableUdpSession
	{
	public:
		using Clock = ReliableUdpSendWindow::Clock;
		using TimePoint = ReliableUdpSendWindow::TimePoint;
		using Duration = ReliableUdpSendWindow::Duration;
		using ResendPacketList = ReliableUdpSendWindow::ResendPacketList;

	private:
		ReliableAckTracker ackTracker_;
		ReliableUdpSendWindow sendWindow_;

	public:
		ReliableUdpSession() = default;
		~ReliableUdpSession() noexcept = default;

		ReliableUdpSession(const ReliableUdpSession&) = delete;
		ReliableUdpSession& operator=(const ReliableUdpSession&) = delete;

		ReliableUdpSession(ReliableUdpSession&&) noexcept = default;
		ReliableUdpSession& operator=(ReliableUdpSession&&) noexcept = default;

	public:
		void Reset() noexcept
		{
			ackTracker_.Reset();
			sendWindow_.Reset();
		}

		[[nodiscard]] ReliableSequence AllocateOutgoingSequence() noexcept
		{
			return sendWindow_.AllocateSequence();
		}

		[[nodiscard]] ReliableUdpPacketHeader BuildOutgoingHeader(ReliableSequence sequence) const noexcept
		{
			ReliableUdpPacketHeader reliableHeader{};
			reliableHeader.sequence = sequence;

			if (ackTracker_.HasReceivedAnySequence())
			{
				reliableHeader.ackSequence = ackTracker_.GetLatestReceivedSequence();
				reliableHeader.ackBitfield = ackTracker_.GetAckBitfield();
			}

			return reliableHeader;
		}

		[[nodiscard]] bool RegisterSentPacket(ReliableSequence sequence, common::packet::PacketBuffer packetBuffer, TimePoint sentTime)
		{
			return sendWindow_.RegisterSentPacket(sequence, std::move(packetBuffer), sentTime);
		}

		void ProcessReceivedHeader(const ReliableUdpPacketHeader& reliableHeader)
		{
			ackTracker_.ObserveReceivedSequence(reliableHeader.sequence);
			sendWindow_.ProcessAck(reliableHeader.ackSequence, reliableHeader.ackBitfield);
		}

		[[nodiscard]] ResendPacketList ExtractResendPackets(TimePoint currentTime)
		{
			return sendWindow_.ExtractResendPackets(currentTime);
		}

	public:
		void SetMaxPendingPacketCount(std::size_t maxPendingPacketCount) noexcept
		{
			sendWindow_.SetMaxPendingPacketCount(maxPendingPacketCount);
		}

		void SetResendInterval(Duration resendInterval) noexcept
		{
			sendWindow_.SetResendInterval(resendInterval);
		}

		[[nodiscard]] ReliableSequence GetNextSequence() const noexcept
		{
			return sendWindow_.GetNextSequence();
		}

		[[nodiscard]] std::size_t GetPendingPacketCount() const noexcept
		{
			return sendWindow_.GetPendingPacketCount();
		}

		[[nodiscard]] bool HasReceivedSequence(ReliableSequence sequence) const noexcept
		{
			return ackTracker_.IsSequenceAcked(sequence);
		}

		[[nodiscard]] bool HasReceivedAnySequence() const noexcept
		{
			return ackTracker_.HasReceivedAnySequence();
		}

		[[nodiscard]] ReliableSequence GetLatestReceivedSequence() const noexcept
		{
			return ackTracker_.GetLatestReceivedSequence();
		}

		[[nodiscard]] std::uint32_t GetAckBitfield() const noexcept
		{
			return ackTracker_.GetAckBitfield();
		}
	};
}