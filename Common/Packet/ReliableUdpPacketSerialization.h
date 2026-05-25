#pragma once
#pragma once

#include <optional>

#include <Common/Net/ReliableUdpPacketHeader.h>
#include <Common/Packet/PacketSerialization.h>

namespace common::packet
{
	inline void WriteReliableUdpPacketHeader(PacketWriter& writer, const net::ReliableUdpPacketHeader& reliableHeader)
	{
		writer.WriteUInt32(reliableHeader.sequence);
		writer.WriteUInt32(reliableHeader.ackSequence);
		writer.WriteUInt32(reliableHeader.ackBitfield);
	}

	[[nodiscard]] inline bool ReadReliableUdpPacketHeader(PacketReader& reader, net::ReliableUdpPacketHeader& reliableHeader) noexcept
	{
		return reader.ReadUInt32(reliableHeader.sequence)
			&& reader.ReadUInt32(reliableHeader.ackSequence)
			&& reader.ReadUInt32(reliableHeader.ackBitfield);
	}

	[[nodiscard]] inline std::optional<net::ReliableUdpPacketHeader> DeserializeReliableUdpPacketHeader(
		const char* packetData,
		int packetSize
	)
	{
		if (packetData == nullptr || packetSize < static_cast<int>(net::reliableUdpPacketHeaderWireSize))
		{
			return std::nullopt;
		}

		PacketReader reader(packetData, packetSize);
		net::ReliableUdpPacketHeader reliableHeader{};

		if (!ReadReliableUdpPacketHeader(reader, reliableHeader))
		{
			return std::nullopt;
		}

		return reliableHeader;
	}
}