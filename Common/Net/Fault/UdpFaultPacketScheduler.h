#pragma once

#include <WinSock2.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include <Common/Packet/PacketBuffer.h>
#include <Common/Time/TimeTypes.h>

#include "UdpFaultDecisionGenerator.h"

namespace common::net
{
	class UdpFaultPacketScheduler
	{
	public:
		struct Packet
		{
		public:
			sockaddr_in remoteAddress{};
			packet::PacketBuffer packetBuffer;
		};

	public:
		using PacketList = std::vector<Packet>;

	public:
		struct SubmitResult
		{
		public:
			bool dropped = false;
			PacketList readyPacketList;
		};

	public:
		using Decision = UdpFaultDecisionGenerator::Decision;

	private:
		using ScheduleKey = std::pair<time::TimePoint, std::uint64_t>;
		using PendingPacketMap = std::map<ScheduleKey, Packet>;

		mutable std::mutex schedulerMutex_;
		PendingPacketMap pendingPacketMap_;
		std::uint64_t nextScheduleOrder_ = 0;

	public:
		UdpFaultPacketScheduler() = default;
		~UdpFaultPacketScheduler() noexcept = default;

		UdpFaultPacketScheduler(const UdpFaultPacketScheduler&) = delete;
		UdpFaultPacketScheduler& operator=(const UdpFaultPacketScheduler&) = delete;

		UdpFaultPacketScheduler(UdpFaultPacketScheduler&&) = delete;
		UdpFaultPacketScheduler& operator=(UdpFaultPacketScheduler&&) = delete;

	public:
		[[nodiscard]] SubmitResult Submit(
			const sockaddr_in& remoteAddress,
			packet::ConstPacketSpan packetData,
			const Decision& decision,
			time::TimePoint currentTime
		);

		[[nodiscard]] PacketList ExtractReadyPackets(time::TimePoint currentTime);

		void Reset() noexcept;

	private:
		[[nodiscard]] static Packet CreatePacket(const sockaddr_in& remoteAddress, packet::ConstPacketSpan packetData);

	public:
		[[nodiscard]] std::size_t GetPendingPacketCount() const noexcept;
	};
}