#include "ClientPacketDispatcher.h"

#include <optional>
#include <type_traits>
#include <utility>

#include <Common/Packet/PacketSerialization.h>

namespace client::net
{
	std::size_t ClientPacketDispatcher::PacketTypeHasher::operator()(common::packet::PacketType packetType) const noexcept
	{
		using UnderlyingType = std::underlying_type_t<common::packet::PacketType>;
		return static_cast<std::size_t>(static_cast<UnderlyingType>(packetType));
	}

	void ClientPacketDispatcher::Clear() noexcept
	{
		handlerTable_.clear();
	}

	void ClientPacketDispatcher::RegisterHandler(common::packet::PacketType packetType, int expectedPacketSize, PacketHandler packetHandler)
	{
		if (expectedPacketSize < 0 || !packetHandler)
		{
			return;
		}

		HandlerEntry handlerEntry{};
		handlerEntry.expectedPacketSize = expectedPacketSize;
		handlerEntry.packetHandler = std::move(packetHandler);

		handlerTable_[packetType] = std::move(handlerEntry);
	}

	void ClientPacketDispatcher::Dispatch(const char* packetData, int packetSize) const
	{
		const std::optional<common::packet::PacketHeader> packetHeader = common::packet::DeserializePacketHeader(packetData, packetSize);

		if (!packetHeader.has_value())
		{
			return;
		}

		if (packetHeader->size != packetSize)
		{
			return;
		}

		if (packetHeader->version != common::packet::protocolVersion)
		{
			return;
		}

		const auto handlerIterator = handlerTable_.find(packetHeader->type);
		if (handlerIterator == handlerTable_.end())
		{
			return;
		}

		const HandlerEntry& handlerEntry = handlerIterator->second;
		if (handlerEntry.expectedPacketSize > 0 && handlerEntry.expectedPacketSize != packetSize)
		{
			return;
		}

		handlerEntry.packetHandler(packetData, packetSize);
	}
}