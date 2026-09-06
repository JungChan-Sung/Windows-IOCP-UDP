#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include <Common/Packet/PacketBuffer.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/PacketHeader.h>
#include <Common/Packet/PacketType.h>
#include <Common/Packet/Serialization/PacketCodec.h>
#include <Common/Packet/Serialization/PacketReader.h>
#include <Common/Packet/Serialization/PacketWriter.h>

namespace common::packet
{
	[[nodiscard]] inline std::uint16_t MakePacketHeaderVersion(bool isReliable, bool isAuthenticated = false) noexcept
	{
		std::uint16_t version = protocolVersion;

		if (isReliable)
		{
			version |= packetHeaderReliableFlag;
		}

		if (isAuthenticated)
		{
			version |= packetHeaderAuthenticatedFlag;
		}

		return version;
	}

	[[nodiscard]] inline bool IsReliablePacketHeader(const PacketHeader& packetHeader) noexcept
	{
		return (packetHeader.version & packetHeaderReliableFlag) != 0;
	}

	[[nodiscard]] inline bool IsAuthenticatedPacketHeader(const PacketHeader& packetHeader) noexcept
	{
		return (packetHeader.version & packetHeaderAuthenticatedFlag) != 0;
	}

	[[nodiscard]] inline std::uint16_t GetPacketHeaderProtocolVersion(const PacketHeader& packetHeader) noexcept
	{
		return packetHeader.version & packetHeaderVersionMask;
	}

	inline void WritePacketHeader(PacketWriter& writer, std::uint16_t packetSize, PacketType packetType, bool isReliable = false, bool isAuthenticated = false)
	{
		writer.WriteUInt16(packetSize);
		writer.WriteUInt16(static_cast<std::uint16_t>(packetType));
		writer.WriteUInt16(MakePacketHeaderVersion(isReliable, isAuthenticated));
	}

	[[nodiscard]] inline bool ReadPacketHeader(PacketReader& reader, PacketHeader& packetHeader) noexcept
	{
		if (!reader.ReadUInt16(packetHeader.size))
		{
			return false;
		}

		std::uint16_t rawPacketType = 0;
		if (!reader.ReadUInt16(rawPacketType))
		{
			return false;
		}

		packetHeader.type = static_cast<PacketType>(rawPacketType);
		if (!reader.ReadUInt16(packetHeader.version))
		{
			return false;
		}

		return true;
	}

	[[nodiscard]] inline std::optional<PacketHeader> DeserializePacketHeader(const char* packetData, int packetSize)
	{
		if (packetData == nullptr || packetSize < static_cast<int>(serializedPacketHeaderSize))
		{
			return std::nullopt;
		}

		PacketReader reader(packetData, packetSize);
		PacketHeader packetHeader{};
		if (!ReadPacketHeader(reader, packetHeader))
		{
			return std::nullopt;
		}

		return packetHeader;
	}

	[[nodiscard]] inline std::optional<PacketBuffer> FinishSerializedPacket(PacketWriter& writer, std::size_t expectedSize)
	{
		if (!writer.IsValid())
		{
			return std::nullopt;
		}

		if (writer.GetBuffer().size() != expectedSize)
		{
			return std::nullopt;
		}

		if (writer.GetBuffer().size() > maxSerializedPacketSize)
		{
			return std::nullopt;
		}

		if (writer.GetBuffer().size() > std::numeric_limits<std::uint16_t>::max())
		{
			return std::nullopt;
		}

		return writer.TakeBuffer();
	}

	// 기본 패킷이 예상 타입·크기·프로토콜을 만족하며 전송 계층의 Reliable/Auth Wrapping이 제거된 상태인지 검증하는 함수
	[[nodiscard]] inline bool ReadExpectedHeader(
		PacketReader& reader,
		PacketHeader& packetHeader,
		PacketType expectedPacketType,
		int packetSize
	) noexcept
	{
		if (!ReadPacketHeader(reader, packetHeader))
		{
			return false;
		}

		if (packetHeader.size != packetSize)
		{
			return false;
		}

		if (packetHeader.type != expectedPacketType)
		{
			return false;
		}

		if (GetPacketHeaderProtocolVersion(packetHeader) != protocolVersion)
		{
			return false;
		}

		if (IsReliablePacketHeader(packetHeader) || IsAuthenticatedPacketHeader(packetHeader))
		{
			return false;
		}

		return true;
	}

	template <typename TPacket>
	[[nodiscard]] std::optional<PacketBuffer> SerializePacket(const TPacket& packet)
	{
		const std::size_t serializedSize = PacketCodec<TPacket>::GetSerializedSize(packet);
		if (serializedSize > maxSerializedPacketSize)
		{
			return std::nullopt;
		}

		if (serializedSize > std::numeric_limits<std::uint16_t>::max())
		{
			return std::nullopt;
		}

		PacketWriter writer;
		writer.Reserve(serializedSize);

		WritePacketHeader(writer, static_cast<std::uint16_t>(serializedSize), PacketCodec<TPacket>::packetType);
		PacketCodec<TPacket>::WritePayload(writer, packet);

		return FinishSerializedPacket(writer, serializedSize);
	}

	template <typename TPacket>
	[[nodiscard]] std::optional<TPacket> DeserializePacket(const char* packetData, int packetSize)
	{
		if (packetData == nullptr || packetSize < static_cast<int>(serializedPacketHeaderSize))
		{
			return std::nullopt;
		}

		if (PacketCodec<TPacket>::fixedWireSize > 0 && packetSize != PacketCodec<TPacket>::fixedWireSize)
		{
			return std::nullopt;
		}

		PacketReader reader(packetData, packetSize);
		TPacket packet{};
		if (!ReadExpectedHeader(reader, packet.header, PacketCodec<TPacket>::packetType, packetSize))
		{
			return std::nullopt;
		}

		if (!PacketCodec<TPacket>::ReadPayload(reader, packet))
		{
			return std::nullopt;
		}

		if (PacketCodec<TPacket>::GetSerializedSize(packet) != static_cast<std::size_t>(packetSize))
		{
			return std::nullopt;
		}

		if (!reader.IsComplete())
		{
			return std::nullopt;
		}

		return packet;
	}
}