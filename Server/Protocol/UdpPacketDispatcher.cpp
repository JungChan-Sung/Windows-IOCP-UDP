#include "UdpPacketDispatcher.h"

#include <optional>
#include <type_traits>
#include <utility>

#include <Common/Packet/PacketSerialization.h>

namespace server::protocol
{
	std::size_t UdpPacketDispatcher::PacketTypeHasher::operator()(common::packet::PacketType packetType) const noexcept
	{
		using UnderlyingType = std::underlying_type_t<common::packet::PacketType>;
		return static_cast<std::size_t>(static_cast<UnderlyingType>(packetType));
	}

	const char* UdpPacketDispatcher::ToString(DispatchStatus dispatchStatus) noexcept
	{
		switch (dispatchStatus)
		{
		case DispatchStatus::Succeeded:
			return "Succeeded";

		case DispatchStatus::NullPacketData:
			return "NullPacketData";

		case DispatchStatus::PacketTooSmall:
			return "PacketTooSmall";

		case DispatchStatus::InvalidPacketHeader:
			return "InvalidPacketHeader";

		case DispatchStatus::InvalidHeaderSize:
			return "InvalidHeaderSize";

		case DispatchStatus::UnsupportedProtocolVersion:
			return "UnsupportedProtocolVersion";

		case DispatchStatus::UnknownPacketType:
			return "UnknownPacketType";

		case DispatchStatus::InvalidPacketSize:
			return "InvalidPacketSize";

		case DispatchStatus::EmptyHandler:
			return "EmptyHandler";

		case DispatchStatus::InvalidPacketPayload:
			return "InvalidPacketPayload";
		default:
			return "Unknown";
		}
	}

	void UdpPacketDispatcher::Clear() noexcept
	{
		handlerTable_.clear();
	}

	void UdpPacketDispatcher::RegisterHandler(common::packet::PacketType packetType, int expectedPacketSize, PacketProcessor packetProcessor)
	{
		HandlerEntry handlerEntry{};
		handlerEntry.expectedPacketSize = expectedPacketSize;
		handlerEntry.packetProcessor = std::move(packetProcessor);

		handlerTable_.insert_or_assign(packetType, std::move(handlerEntry));
	}

	UdpPacketDispatcher::DispatchResult UdpPacketDispatcher::Dispatch(const sockaddr_in& remoteAddress, const char* packetData, int packetSize) const
	{
		DispatchResult dispatchResult{};
		dispatchResult.actualPacketSize = packetSize;

		if (packetData == nullptr)
		{
			dispatchResult.status = DispatchStatus::NullPacketData;
			return dispatchResult;
		}

		if (packetSize < static_cast<int>(common::packet::serializedPacketHeaderSize))
		{
			dispatchResult.status = DispatchStatus::PacketTooSmall;
			return dispatchResult;
		}

		const std::optional<common::packet::PacketHeader> packetHeader
			= common::packet::DeserializePacketHeader(packetData, packetSize);

		if (!packetHeader.has_value())
		{
			dispatchResult.status = DispatchStatus::PacketTooSmall;
			return dispatchResult;
		}

		dispatchResult.packetType = packetHeader->type;
		dispatchResult.declaredPacketSize = packetHeader->size;
		dispatchResult.protocolVersion = packetHeader->version;

		if (packetHeader->size != packetSize)
		{
			dispatchResult.status = DispatchStatus::InvalidHeaderSize;
			return dispatchResult;
		}

		if (packetHeader->version != common::packet::protocolVersion)
		{
			dispatchResult.status = DispatchStatus::UnsupportedProtocolVersion;
			return dispatchResult;
		}

		const auto handlerIterator = handlerTable_.find(packetHeader->type);
		if (handlerIterator == handlerTable_.end())
		{
			dispatchResult.status = DispatchStatus::UnknownPacketType;
			return dispatchResult;
		}

		const HandlerEntry& handlerEntry = handlerIterator->second;
		dispatchResult.expectedPacketSize = handlerEntry.expectedPacketSize;

		if (handlerEntry.expectedPacketSize > 0 && packetSize != handlerEntry.expectedPacketSize)
		{
			dispatchResult.status = DispatchStatus::InvalidPacketSize;
			return dispatchResult;
		}

		if (!handlerEntry.packetProcessor)
		{
			dispatchResult.status = DispatchStatus::EmptyHandler;
			return dispatchResult;
		}

		const PacketProcessResult processResult = handlerEntry.packetProcessor(
			remoteAddress,
			packetData,
			packetSize
		);

		dispatchResult.status = processResult.status;
		dispatchResult.detailCode = processResult.detailCode;
		return dispatchResult;
	}
}