#pragma once

#include <cstddef>

#include <Common/Packet/Game/SessionPacket.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/Serialization/PacketCodec.h>
#include <Common/Packet/Serialization/PacketReader.h>
#include <Common/Packet/Serialization/PacketWriter.h>

namespace common::packet
{
	template <>
	struct PacketCodec<JoinRequestPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::JoinRequest;
		static inline constexpr int fixedWireSize = static_cast<int>(
			serializedPacketHeaderSize
			+ uint64WireSize
			+ uint64WireSize
			);

	public:
		static void WritePayload(PacketWriter& writer, const JoinRequestPacket& packet)
		{
			writer.WriteUInt64(packet.sessionToken.high);
			writer.WriteUInt64(packet.sessionToken.low);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, JoinRequestPacket& packet) noexcept
		{
			return reader.ReadUInt64(packet.sessionToken.high)
				&& reader.ReadUInt64(packet.sessionToken.low);
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const JoinRequestPacket&) noexcept
		{
			return fixedWireSize;
		}
	};

	template <>
	struct PacketCodec<JoinResponsePacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::JoinResponse;
		static inline constexpr int fixedWireSize = static_cast<int>(
			serializedPacketHeaderSize
			+ uint32WireSize
			+ int32WireSize
			+ floatWireSize
			+ floatWireSize
			);

	public:
		static void WritePayload(PacketWriter& writer, const JoinResponsePacket& packet)
		{
			writer.WriteUInt32(packet.playerId);
			writer.WriteInt32(packet.roomId);
			writer.WriteFloat(packet.spawnX);
			writer.WriteFloat(packet.spawnY);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, JoinResponsePacket& packet) noexcept
		{
			return reader.ReadUInt32(packet.playerId)
				&& reader.ReadInt32(packet.roomId)
				&& reader.ReadFloat(packet.spawnX)
				&& reader.ReadFloat(packet.spawnY);
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const JoinResponsePacket&) noexcept
		{
			return fixedWireSize;
		}
	};

	template <>
	struct PacketCodec<LeaveRequestPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::LeaveRequest;
		static inline constexpr int fixedWireSize = static_cast<int>(serializedPacketHeaderSize);

	public:
		static void WritePayload(PacketWriter&, const LeaveRequestPacket&)
		{}

		[[nodiscard]] static bool ReadPayload(PacketReader&, LeaveRequestPacket&) noexcept
		{
			return true;
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const LeaveRequestPacket&) noexcept
		{
			return fixedWireSize;
		}
	};

}