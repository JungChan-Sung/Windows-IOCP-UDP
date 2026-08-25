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
}