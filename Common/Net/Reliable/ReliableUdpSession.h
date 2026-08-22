#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <utility>


#include <Common/Packet/PacketReliability.h>
#include <Common/Net/Reliable/ReliableUdpConfig.h>
#include <Common/Net/Reliable/ReliableUdpPacketBuilder.h>
#include <Common/Net/Reliable/ReliableUdpPacketHeader.h>
#include <Common/Net/Reliable/ReliableUdpProtocol.h>
#include <Common/Net/Reliable/ReliableUdpSendWindow.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Packet/Serialization/PacketSerializationCore.h>
#include <Common/Time/TimeTypes.h>

namespace common::net
{
	class ReliableUdpSession
	{
	public:
		enum class BuildOutgoingPacketFailure
		{
			InvalidGamePacket,
			SendWindowFull,
		};

		enum class ProcessReceivedPacketStatus
		{
			AckOnlyProcessed,
			InvalidAck,
			DataReceived,
			DuplicateData,
		};

		struct ProcessReceivedPacketResult
		{
		public:
			ProcessReceivedPacketStatus status = ProcessReceivedPacketStatus::AckOnlyProcessed;
			std::optional<packet::PacketBuffer> ackPacketBuffer;
		};

	public:
		using Clock = time::Clock;
		using TimePoint = time::TimePoint;
		using Duration = time::Duration;
		using ResendPacketList = ReliableUdpSendWindow::ResendPacketList;
		using ResendResult = ReliableUdpSendWindow::ResendResult;
		using BuildOutgoingPacketResult = std::expected<packet::PacketBuffer, BuildOutgoingPacketFailure>;

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
		void Configure(const ReliableUdpConfig& config) noexcept
		{
			sendWindow_.SetMaxPendingPacketCount(config.maxPendingPacketCount);
			sendWindow_.SetMaxResendCount(config.maxResendCount);
			sendWindow_.SetResendInterval(config.resendInterval);
		}

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

		[[nodiscard]] BuildOutgoingPacketResult BuildOutgoingPacket(packet::ConstPacketSpan serializedGamePacket, TimePoint currentTime)
		{
			const std::optional<packet::PacketHeader> packetHeader = packet::DeserializePacketHeader(serializedGamePacket.data(), static_cast<int>(serializedGamePacket.size()));
			if (!packetHeader.has_value()
				|| packet::IsReliablePacketHeader(*packetHeader)
				|| packet::GetPacketHeaderProtocolVersion(*packetHeader) != packet::protocolVersion
				|| static_cast<std::size_t>(packetHeader->size) != serializedGamePacket.size()
				|| !packet::IsReliablePacketType(packetHeader->type))
			{
				return std::unexpected(BuildOutgoingPacketFailure::InvalidGamePacket);
			}

			const ReliableSequence sequence = AllocateOutgoingSequence();
			const ReliableUdpPacketHeader reliableHeader = BuildOutgoingHeader(sequence);
			std::optional<packet::PacketBuffer> reliablePacketBuffer = BuildReliableUdpPacket(reliableHeader, serializedGamePacket);
			if (!reliablePacketBuffer.has_value())
			{
				return std::unexpected(BuildOutgoingPacketFailure::InvalidGamePacket);
			}

			if (!RegisterSentPacket(sequence, *reliablePacketBuffer, currentTime))
			{
				return std::unexpected(BuildOutgoingPacketFailure::SendWindowFull);
			}

			return std::move(*reliablePacketBuffer);
		}

		[[nodiscard]] bool RegisterSentPacket(ReliableSequence sequence, packet::PacketBuffer packetBuffer, TimePoint sentTime)
		{
			return sendWindow_.RegisterSentPacket(sequence, std::move(packetBuffer), sentTime);
		}

		[[nodiscard]] ProcessReceivedPacketResult ProcessReceivedPacket(const ReliableUdpPacketView& packetView)
		{
			if (packetView.packetHeader.type == packet::PacketType::None)
			{
				if (!ProcessReceivedAck(packetView.reliableHeader))
				{
					return ProcessReceivedPacketResult{
						.status = ProcessReceivedPacketStatus::InvalidAck,
					};
				}

				return ProcessReceivedPacketResult{
					.status = ProcessReceivedPacketStatus::AckOnlyProcessed,
				};
			}

			const bool isNewPacket = ProcessReceivedDataHeader(packetView.reliableHeader);
			return ProcessReceivedPacketResult{
				.status = isNewPacket ? ProcessReceivedPacketStatus::DataReceived : ProcessReceivedPacketStatus::DuplicateData,
				.ackPacketBuffer = BuildAckPacket(),
			};
		}

		[[nodiscard]] bool ProcessReceivedAck(const ReliableUdpPacketHeader& reliableHeader)
		{
			return sendWindow_.ProcessAck(reliableHeader.ackSequence, reliableHeader.ackBitfield);
		}

		[[nodiscard]] bool ProcessReceivedDataHeader(const ReliableUdpPacketHeader& reliableHeader)
		{
			const bool isAlreadyReceived = ackTracker_.IsSequenceAcked(reliableHeader.sequence);

			ackTracker_.ObserveReceivedSequence(reliableHeader.sequence);
			sendWindow_.ProcessAck(reliableHeader.ackSequence, reliableHeader.ackBitfield);

			return !isAlreadyReceived;
		}

		[[nodiscard]] ReliableUdpPacketHeader BuildOutgoingAckHeader() const noexcept
		{
			ReliableUdpPacketHeader reliableHeader{};

			if (ackTracker_.HasReceivedAnySequence())
			{
				reliableHeader.ackSequence = ackTracker_.GetLatestReceivedSequence();
				reliableHeader.ackBitfield = ackTracker_.GetAckBitfield();
			}

			return reliableHeader;
		}

		[[nodiscard]] std::optional<packet::PacketBuffer> BuildAckPacket() const
		{
			return BuildReliableUdpAckPacket(BuildOutgoingAckHeader());
		}

		[[nodiscard]] ResendResult ExtractResendResult(TimePoint currentTime)
		{
			return sendWindow_.ExtractResendResult(currentTime);
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

		void SetMaxResendCount(int maxResendCount) noexcept
		{
			sendWindow_.SetMaxResendCount(maxResendCount);
		}

		void SetResendInterval(Duration resendInterval) noexcept
		{
			sendWindow_.SetResendInterval(resendInterval);
		}

		[[nodiscard]] int GetMaxResendCount() const noexcept
		{
			return sendWindow_.GetMaxResendCount();
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