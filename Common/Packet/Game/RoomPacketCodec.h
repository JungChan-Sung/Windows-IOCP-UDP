#pragma once

#include <cstddef>

#include <Common/Packet/Game/RoomPacket.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/Serialization/PacketCodec.h>
#include <Common/Packet/Serialization/PacketReader.h>
#include <Common/Packet/Serialization/PacketWriter.h>

namespace common::packet
{
	template <>
	struct PacketCodec<JoinRoomRequestPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::JoinRoomRequest;
		static inline constexpr int fixedWireSize = static_cast<int>(
			serializedPacketHeaderSize
			+ int32WireSize
			);

	public:
		static void WritePayload(PacketWriter& writer, const JoinRoomRequestPacket& packet)
		{
			writer.WriteInt32(packet.roomId);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, JoinRoomRequestPacket& packet) noexcept
		{
			return reader.ReadInt32(packet.roomId);
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const JoinRoomRequestPacket&) noexcept
		{
			return fixedWireSize;
		}
	};

	template <>
	struct PacketCodec<JoinRoomResponsePacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::JoinRoomResponse;
		static inline constexpr int fixedWireSize = static_cast<int>(
			serializedPacketHeaderSize
			+ int32WireSize
			+ floatWireSize
			+ floatWireSize
			);

	public:
		static void WritePayload(PacketWriter& writer, const JoinRoomResponsePacket& packet)
		{
			writer.WriteInt32(packet.roomId);
			writer.WriteFloat(packet.spawnX);
			writer.WriteFloat(packet.spawnY);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, JoinRoomResponsePacket& packet) noexcept
		{
			return reader.ReadInt32(packet.roomId)
				&& reader.ReadFloat(packet.spawnX)
				&& reader.ReadFloat(packet.spawnY);
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const JoinRoomResponsePacket&) noexcept
		{
			return fixedWireSize;
		}
	};

	template <>
	struct PacketCodec<PlayerJoinedPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::PlayerJoined;
		static inline constexpr int fixedWireSize = static_cast<int>(
			serializedPacketHeaderSize
			+ uint32WireSize
			+ int32WireSize
			+ floatWireSize
			+ floatWireSize
			);

	public:
		static void WritePayload(PacketWriter& writer, const PlayerJoinedPacket& packet)
		{
			writer.WriteUInt32(packet.playerId);
			writer.WriteInt32(packet.roomId);
			writer.WriteFloat(packet.x);
			writer.WriteFloat(packet.y);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, PlayerJoinedPacket& packet) noexcept
		{
			return reader.ReadUInt32(packet.playerId)
				&& reader.ReadInt32(packet.roomId)
				&& reader.ReadFloat(packet.x)
				&& reader.ReadFloat(packet.y);
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const PlayerJoinedPacket&) noexcept
		{
			return fixedWireSize;
		}
	};

	template <>
	struct PacketCodec<PlayerLeftPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::PlayerLeft;
		static inline constexpr int fixedWireSize = static_cast<int>(
			serializedPacketHeaderSize
			+ uint32WireSize
			+ int32WireSize
			);

	public:
		static void WritePayload(PacketWriter& writer, const PlayerLeftPacket& packet)
		{
			writer.WriteUInt32(packet.playerId);
			writer.WriteInt32(packet.roomId);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, PlayerLeftPacket& packet) noexcept
		{
			return reader.ReadUInt32(packet.playerId)
				&& reader.ReadInt32(packet.roomId);
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const PlayerLeftPacket&) noexcept
		{
			return fixedWireSize;
		}
	};

}