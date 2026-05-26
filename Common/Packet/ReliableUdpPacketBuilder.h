#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <Common/Net/ReliableUdpPacketHeader.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Packet/ReliableUdpPacketSerialization.h>

namespace common::packet
{
	struct ReliableUdpPacketView
	{
	public:
		net::ReliableUdpPacketHeader reliableHeader{};
		std::span<const char> payload;
	};

	[[nodiscard]] inline std::optional<PacketBuffer> BuildReliableUdpPacket(
		const net::ReliableUdpPacketHeader& reliableHeader,
		std::span<const char> payload
	)
	{
		const std::size_t packetSize = net::reliableUdpPacketHeaderWireSize + payload.size();

		if (payload.empty())
		{
			return std::nullopt;
		}

		if (packetSize > maxSerializedPacketSize)
		{
			return std::nullopt;
		}

		PacketWriter writer;
		writer.Reserve(packetSize);

		WriteReliableUdpPacketHeader(writer, reliableHeader);

		PacketBuffer packetBuffer = writer.TakeBuffer();
		packetBuffer.insert(packetBuffer.end(), payload.begin(), payload.end());

		if (packetBuffer.size() != packetSize)
		{
			return std::nullopt;
		}

		return packetBuffer;
	}

	[[nodiscard]] inline std::optional<ReliableUdpPacketView> ParseReliableUdpPacket(const char* packetData, int packetSize)
	{
		if (packetData == nullptr || packetSize <= static_cast<int>(net::reliableUdpPacketHeaderWireSize))
		{
			return std::nullopt;
		}

		const std::optional<net::ReliableUdpPacketHeader> reliableHeader = DeserializeReliableUdpPacketHeader(packetData, packetSize);

		if (!reliableHeader.has_value())
		{
			return std::nullopt;
		}

		const char* payloadData = packetData + net::reliableUdpPacketHeaderWireSize;
		const int payloadSize = packetSize - static_cast<int>(net::reliableUdpPacketHeaderWireSize);

		ReliableUdpPacketView packetView{};
		packetView.reliableHeader = *reliableHeader;
		packetView.payload = std::span<const char>(payloadData, static_cast<std::size_t>(payloadSize));

		return packetView;
	}
}