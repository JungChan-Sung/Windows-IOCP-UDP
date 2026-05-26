#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <span>

#include <Common/Net/ReliableUdpPacketHeader.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/PacketHeader.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Packet/ReliableUdpPacketSerialization.h>

namespace common::packet
{
	inline constexpr std::size_t reliableUdpPacketHeaderOffset = serializedPacketHeaderSize;
	inline constexpr std::size_t reliableUdpPayloadOffset = serializedPacketHeaderSize + net::reliableUdpPacketHeaderWireSize;

	struct ReliableUdpPacketView
	{
	public:
		PacketHeader packetHeader{};
		net::ReliableUdpPacketHeader reliableHeader{};
		std::span<const char> payload;
	};

	[[nodiscard]] inline std::optional<PacketBuffer> BuildReliableUdpPacket(
		const net::ReliableUdpPacketHeader& reliableHeader,
		std::span<const char> serializedGamePacket
	)
	{
		if (serializedGamePacket.size() <= serializedPacketHeaderSize)
		{
			return std::nullopt;
		}

		const std::optional<PacketHeader> gamePacketHeader = DeserializePacketHeader(
			serializedGamePacket.data(),
			static_cast<int>(serializedGamePacket.size())
		);

		if (!gamePacketHeader.has_value())
		{
			return std::nullopt;
		}

		if (IsReliablePacketHeader(*gamePacketHeader))
		{
			return std::nullopt;
		}

		if (GetPacketHeaderProtocolVersion(*gamePacketHeader) != protocolVersion)
		{
			return std::nullopt;
		}

		if (gamePacketHeader->size != serializedGamePacket.size())
		{
			return std::nullopt;
		}

		const std::size_t payloadSize = serializedGamePacket.size() - serializedPacketHeaderSize;
		const std::size_t reliablePacketSize = reliableUdpPayloadOffset + payloadSize;

		if (reliablePacketSize > maxSerializedPacketSize)
		{
			return std::nullopt;
		}

		if (reliablePacketSize > std::numeric_limits<std::uint16_t>::max())
		{
			return std::nullopt;
		}

		PacketWriter writer;
		writer.Reserve(reliablePacketSize);

		WritePacketHeader(writer, static_cast<std::uint16_t>(reliablePacketSize), gamePacketHeader->type, true);

		WriteReliableUdpPacketHeader(writer, reliableHeader);

		PacketBuffer packetBuffer = writer.TakeBuffer();

		const std::span<const char> payload = serializedGamePacket.subspan(serializedPacketHeaderSize);
		packetBuffer.insert(packetBuffer.end(), payload.begin(), payload.end());

		if (packetBuffer.size() != reliablePacketSize)
		{
			return std::nullopt;
		}

		return packetBuffer;
	}

	[[nodiscard]] inline std::optional<ReliableUdpPacketView> ParseReliableUdpPacket(const char* packetData, int packetSize)
	{
		if (packetData == nullptr || packetSize <= static_cast<int>(reliableUdpPayloadOffset))
		{
			return std::nullopt;
		}

		PacketReader reader(packetData, packetSize);

		PacketHeader packetHeader{};
		if (!ReadPacketHeader(reader, packetHeader))
		{
			return std::nullopt;
		}

		if (packetHeader.size != packetSize)
		{
			return std::nullopt;
		}

		if (!IsReliablePacketHeader(packetHeader))
		{
			return std::nullopt;
		}

		if (GetPacketHeaderProtocolVersion(packetHeader) != protocolVersion)
		{
			return std::nullopt;
		}

		net::ReliableUdpPacketHeader reliableHeader{};
		if (!ReadReliableUdpPacketHeader(reader, reliableHeader))
		{
			return std::nullopt;
		}

		const char* payloadData = packetData + reliableUdpPayloadOffset;
		const int payloadSize = packetSize - static_cast<int>(reliableUdpPayloadOffset);

		ReliableUdpPacketView packetView{};
		packetView.packetHeader = packetHeader;
		packetView.reliableHeader = reliableHeader;
		packetView.payload = std::span<const char>(payloadData, static_cast<std::size_t>(payloadSize));

		return packetView;
	}

	[[nodiscard]] inline std::optional<PacketBuffer> BuildGamePacketFromReliableUdpPacketView(
		const ReliableUdpPacketView& packetView
	)
	{
		if (packetView.payload.empty())
		{
			return std::nullopt;
		}

		const std::size_t gamePacketSize = serializedPacketHeaderSize + packetView.payload.size();

		if (gamePacketSize > maxSerializedPacketSize)
		{
			return std::nullopt;
		}

		if (gamePacketSize > std::numeric_limits<std::uint16_t>::max())
		{
			return std::nullopt;
		}

		PacketWriter writer;
		writer.Reserve(gamePacketSize);

		WritePacketHeader(
			writer,
			static_cast<std::uint16_t>(gamePacketSize),
			packetView.packetHeader.type
		);

		PacketBuffer packetBuffer = writer.TakeBuffer();
		packetBuffer.insert(packetBuffer.end(), packetView.payload.begin(), packetView.payload.end());

		if (packetBuffer.size() != gamePacketSize)
		{
			return std::nullopt;
		}

		return packetBuffer;
	}
}