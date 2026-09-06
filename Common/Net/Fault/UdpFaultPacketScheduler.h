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
	// Fault 결정에 따라 패킷의 즉시 전송 또는 지연 전송을 스케줄링하는 클래스
	class UdpFaultPacketScheduler
	{
	public:
		// 지연 전송 시 원본 버퍼의 수명에 의존하지 않도록 목적지와 패킷 데이터를 소유하는 구조체
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
		// 동일한 Release Time을 가진 패킷도 모두 보관하고 등록 순서를 유지하도록
		// 단조 증가하는 Schedule Order를 보조 키로 사용
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