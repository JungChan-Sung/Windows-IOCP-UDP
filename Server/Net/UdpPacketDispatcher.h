#pragma once

#include <WinSock2.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <optional>

#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketHeader.h>

namespace server::net
{
	class UdpPacketDispatcher
	{
	public:
		enum class DispatchStatus
		{
			Succeeded,
			NullPacketData,
			PacketTooSmall,
			InvalidPacketHeader,
			InvalidHeaderSize,
			UnsupportedProtocolVersion,
			UnknownPacketType,
			InvalidPacketSize,
			InvalidPacketPayload,
			EmptyHandler,
			Count
		};

		struct DispatchResult
		{
		public:
			DispatchStatus status = DispatchStatus::Succeeded;
			std::optional<common::packet::PacketType> packetType;
			int actualPacketSize = 0;
			int declaredPacketSize = 0;
			int expectedPacketSize = 0;
			std::uint16_t protocolVersion = 0;
			int detailCode = 0;
		};

		struct PacketProcessResult
		{
		public:
			DispatchStatus status = DispatchStatus::Succeeded;
			int detailCode = 0;
		};

	public:
		using PacketProcessor = std::function<PacketProcessResult(const sockaddr_in&, const char*, int)>;

	private:
		struct PacketTypeHasher
		{
		public:
			[[nodiscard]] std::size_t operator()(common::packet::PacketType packetType) const noexcept;
		};

		struct HandlerEntry
		{
		public:
			int expectedPacketSize = 0;
			PacketProcessor packetProcessor;
		};

	private:
		using HandlerTable = std::unordered_map<common::packet::PacketType, HandlerEntry, PacketTypeHasher>;

	private:
		HandlerTable handlerTable_;

	public:
		UdpPacketDispatcher() = default;
		~UdpPacketDispatcher() noexcept = default;

		UdpPacketDispatcher(const UdpPacketDispatcher&) = delete;
		UdpPacketDispatcher& operator=(const UdpPacketDispatcher&) = delete;

		UdpPacketDispatcher(UdpPacketDispatcher&&) = delete;
		UdpPacketDispatcher& operator=(UdpPacketDispatcher&&) = delete;

	public:
		[[nodiscard]] static const char* ToString(DispatchStatus dispatchStatus) noexcept;

	public:
		void Clear() noexcept;

		void RegisterHandler(common::packet::PacketType packetType,	int expectedPacketSize,	PacketProcessor packetProcessor);

		DispatchResult Dispatch(const sockaddr_in& remoteAddress, const char* packetData, int packetSize) const;
	};
}