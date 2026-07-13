#pragma once

#include <optional>

#include <Common/Net/Reliable/ReliableUdpPacketHeader.h>
#include <Common/Packet/PacketSerialization.h>

namespace common::net
{
	inline void WriteReliableUdpPacketHeader(packet::PacketWriter& writer, const net::ReliableUdpPacketHeader& reliableHeader)
	{
		writer.WriteUInt32(reliableHeader.sequence);
		writer.WriteUInt32(reliableHeader.ackSequence);
		writer.WriteUInt32(reliableHeader.ackBitfield);
	}

	[[nodiscard]] inline bool ReadReliableUdpPacketHeader(packet::PacketReader& reader, ReliableUdpPacketHeader& reliableHeader) noexcept
	{
		return reader.ReadUInt32(reliableHeader.sequence)
			&& reader.ReadUInt32(reliableHeader.ackSequence)
			&& reader.ReadUInt32(reliableHeader.ackBitfield);
	}

	[[nodiscard]] inline std::optional<ReliableUdpPacketHeader> DeserializeReliableUdpPacketHeader(const char* packetData, int packetSize)
	{
		if (packetData == nullptr || packetSize < static_cast<int>(reliableUdpPacketHeaderWireSize))
		{
			return std::nullopt;
		}

		packet::PacketReader reader(packetData, packetSize);
		ReliableUdpPacketHeader reliableHeader{};
		if (!ReadReliableUdpPacketHeader(reader, reliableHeader))
		{
			return std::nullopt;
		}

		return reliableHeader;
	}
}