#pragma once

#include <cstddef>

#include <Common/Packet/Control/ControlPacket.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/Serialization/PacketCodec.h>
#include <Common/Packet/Serialization/PacketReader.h>
#include <Common/Packet/Serialization/PacketWriter.h>

namespace common::packet
{
	template <>
	struct PacketCodec<KeepAlivePacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::KeepAlive;
		static inline constexpr int fixedWireSize = static_cast<int>(serializedPacketHeaderSize);

	public:
		static void WritePayload(PacketWriter&, const KeepAlivePacket&)
		{}

		[[nodiscard]] static bool ReadPayload(PacketReader&, KeepAlivePacket&) noexcept
		{
			return true;
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const KeepAlivePacket&) noexcept
		{
			return fixedWireSize;
		}
	};

	template <>
	struct PacketCodec<ServerDisconnectPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::ServerDisconnect;
		static inline constexpr int fixedWireSize = static_cast<int>(serializedPacketHeaderSize + uint8WireSize);

	public:
		static void WritePayload(PacketWriter& writer, const ServerDisconnectPacket& packet)
		{
			writer.WriteUInt8(static_cast<std::uint8_t>(packet.reason));
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, ServerDisconnectPacket& packet) noexcept
		{
			std::uint8_t rawReason = 0;
			if (!reader.ReadUInt8(rawReason))
			{
				return false;
			}

			if (rawReason == static_cast<std::uint8_t>(ServerDisconnectReason::None)
				|| rawReason >= static_cast<std::uint8_t>(ServerDisconnectReason::Count))
			{
				return false;
			}

			packet.reason = static_cast<ServerDisconnectReason>(rawReason);
			return true;
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const ServerDisconnectPacket&) noexcept
		{
			return fixedWireSize;
		}
	};
}