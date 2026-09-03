#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

#include <Common/Net/Reliable/ReliableUdpPacketHeader.h>
#include <Common/Net/Reliable/ReliableUdpPacketSerialization.h>
#include <Common/Packet/Serialization/PacketSerializationCore.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/PacketHeader.h>
#include <Common/Packet/PacketReliability.h>

namespace common::net
{
	inline constexpr std::size_t reliableUdpPacketHeaderOffset = packet::serializedPacketHeaderSize;
	inline constexpr std::size_t reliableUdpPayloadOffset = packet::serializedPacketHeaderSize + reliableUdpPacketHeaderWireSize;

	//┌────────────┐
	//│ PacketHeader           │
	//├────────────┤
	//│ ReliableUdpPacketHeader│
	//│ - sequence             │
	//│ - ackSequence          │
	//│ - ackBitfield          │
	//├────────────┤
	//│ Game Payload           │
	//└────────────┘
	// Reliable 패킷을 파싱한 비소유 뷰 구조체
	// payload는 원본 패킷 메모리를 참조
	struct ReliableUdpPacketView
	{
	public:
		packet::PacketHeader packetHeader{};
		ReliableUdpPacketHeader reliableHeader{};
		packet::ConstPacketSpan payload;
	};

	// 기본 게임 패킷의 Payload를 유지하면서 Reliable 헤더를 삽입한 전송 패킷 생성 함수
	[[nodiscard]] inline std::optional<packet::PacketBuffer> BuildReliableUdpPacket(
		const ReliableUdpPacketHeader& reliableHeader,
		packet::ConstPacketSpan serializedGamePacket
	)
	{
		if (serializedGamePacket.size() < packet::serializedPacketHeaderSize)
		{
			return std::nullopt;
		}

		const std::optional<packet::PacketHeader> gamePacketHeader = packet::DeserializePacketHeader(
			serializedGamePacket.data(),
			static_cast<int>(serializedGamePacket.size())
		);

		if (!gamePacketHeader.has_value())
		{
			return std::nullopt;
		}

		if (packet::IsReliablePacketHeader(*gamePacketHeader) || packet::IsAuthenticatedPacketHeader(*gamePacketHeader))
		{
			return std::nullopt;
		}

		if (packet::GetPacketHeaderProtocolVersion(*gamePacketHeader) != packet::protocolVersion)
		{
			return std::nullopt;
		}

		if (!packet::IsReliablePacketType(gamePacketHeader->type))
		{
			return std::nullopt;
		}

		if (gamePacketHeader->size != serializedGamePacket.size())
		{
			return std::nullopt;
		}

		const std::size_t payloadSize = serializedGamePacket.size() - packet::serializedPacketHeaderSize;
		const std::size_t reliablePacketSize = reliableUdpPayloadOffset + payloadSize;

		if (reliablePacketSize > packet::maxSerializedPacketSize)
		{
			return std::nullopt;
		}

		if (reliablePacketSize > std::numeric_limits<std::uint16_t>::max())
		{
			return std::nullopt;
		}

		packet::PacketWriter writer;
		writer.Reserve(reliablePacketSize);

		packet::WritePacketHeader(writer, static_cast<std::uint16_t>(reliablePacketSize), gamePacketHeader->type, true);

		WriteReliableUdpPacketHeader(writer, reliableHeader);

		packet::PacketBuffer packetBuffer = writer.TakeBuffer();

		const std::span<const char> payload = serializedGamePacket.subspan(packet::serializedPacketHeaderSize);
		packetBuffer.insert(packetBuffer.end(), payload.begin(), payload.end());
		if (packetBuffer.size() != reliablePacketSize)
		{
			return std::nullopt;
		}

		return packetBuffer;
	}

	// 게임 Payload 없이 ACK 정보만 전달하는 Reliable 패킷 생성 함수
	[[nodiscard]] inline std::optional<packet::PacketBuffer> BuildReliableUdpAckPacket(const ReliableUdpPacketHeader& reliableHeader)
	{
		const std::size_t packetSize = reliableUdpPayloadOffset;
		if (packetSize > packet::maxSerializedPacketSize)
		{
			return std::nullopt;
		}

		if (packetSize > std::numeric_limits<std::uint16_t>::max())
		{
			return std::nullopt;
		}

		packet::PacketWriter writer;
		writer.Reserve(packetSize);

		packet::WritePacketHeader(writer, static_cast<std::uint16_t>(packetSize), packet::PacketType::None, true);
		WriteReliableUdpPacketHeader(writer, reliableHeader);

		packet::PacketBuffer packetBuffer = writer.TakeBuffer();
		if (packetBuffer.size() != packetSize)
		{
			return std::nullopt;
		}

		return packetBuffer;
	}

	[[nodiscard]] inline std::optional<ReliableUdpPacketView> ParseReliableUdpPacket(const char* packetData, int packetSize)
	{
		if (packetData == nullptr || packetSize < static_cast<int>(reliableUdpPayloadOffset))
		{
			return std::nullopt;
		}

		packet::PacketReader reader(packetData, packetSize);

		packet::PacketHeader packetHeader{};
		if (!packet::ReadPacketHeader(reader, packetHeader))
		{
			return std::nullopt;
		}

		if (packetHeader.size != packetSize)
		{
			return std::nullopt;
		}

		if (!packet::IsReliablePacketHeader(packetHeader))
		{
			return std::nullopt;
		}

		if (packet::GetPacketHeaderProtocolVersion(packetHeader) != packet::protocolVersion)
		{
			return std::nullopt;
		}

		if (!packet::IsPacketTransportReliabilityValid(packetHeader.type, true))
		{
			return std::nullopt;
		}

		if (packetHeader.type == packet::PacketType::None && packetSize != static_cast<int>(reliableUdpPayloadOffset))
		{
			return std::nullopt;
		}

		ReliableUdpPacketHeader reliableHeader{};
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

	// Reliable 전송 헤더를 제거하고 상위 계층에서 처리할 기본 게임 패킷 복원 함수
	[[nodiscard]] inline std::optional<packet::PacketBuffer> BuildGamePacketFromReliableUdpPacketView(const ReliableUdpPacketView& packetView)
	{
		if (!packet::IsReliablePacketType(packetView.packetHeader.type))
		{
			return std::nullopt;
		}

		const std::size_t gamePacketSize = packet::serializedPacketHeaderSize + packetView.payload.size();

		if (gamePacketSize > packet::maxSerializedPacketSize)
		{
			return std::nullopt;
		}

		if (gamePacketSize > std::numeric_limits<std::uint16_t>::max())
		{
			return std::nullopt;
		}

		packet::PacketWriter writer;
		writer.Reserve(gamePacketSize);

		packet::WritePacketHeader(writer, static_cast<std::uint16_t>(gamePacketSize), packetView.packetHeader.type);

		packet::PacketBuffer packetBuffer = writer.TakeBuffer();
		packetBuffer.insert(packetBuffer.end(), packetView.payload.begin(), packetView.payload.end());

		if (packetBuffer.size() != gamePacketSize)
		{
			return std::nullopt;
		}

		return packetBuffer;
	}
}