#pragma once

#include <cstdint>

#include <Common/Packet/Game/CommandPacket.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/Serialization/PacketCodec.h>
#include <Common/Packet/Serialization/PacketReader.h>
#include <Common/Packet/Serialization/PacketWriter.h>

namespace common::packet
{
	inline void WriteInputFlags(PacketWriter& writer, game::InputFlags inputFlags)
	{
		writer.WriteUInt8(static_cast<std::uint8_t>(inputFlags));
	}

	[[nodiscard]] inline bool ReadInputFlags(PacketReader& reader, game::InputFlags& inputFlags) noexcept
	{
		std::uint8_t rawValue = 0;
		if (!reader.ReadUInt8(rawValue))
		{
			return false;
		}

		inputFlags = static_cast<game::InputFlags>(rawValue);

		return true;
	}

	template <>
	struct PacketCodec<InputCommandPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::InputCommand;
		static inline constexpr int fixedWireSize = static_cast<int>(
			serializedPacketHeaderSize
			+ uint32WireSize
			+ uint8WireSize
			);

	public:
		static void WritePayload(PacketWriter& writer, const InputCommandPacket& packet)
		{
			writer.WriteUInt32(packet.inputSequence);
			WriteInputFlags(writer, packet.inputFlags);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, InputCommandPacket& packet) noexcept
		{
			return reader.ReadUInt32(packet.inputSequence)
				&& ReadInputFlags(reader, packet.inputFlags);
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const InputCommandPacket&) noexcept
		{
			return fixedWireSize;
		}
	};

	template <>
	struct PacketCodec<FireRequestPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::FireRequest;
		static inline constexpr int fixedWireSize = static_cast<int>(serializedPacketHeaderSize);

	public:
		static void WritePayload(PacketWriter&, const FireRequestPacket&)
		{}

		[[nodiscard]] static bool ReadPayload(PacketReader&, FireRequestPacket&) noexcept
		{
			return true;
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const FireRequestPacket&) noexcept
		{
			return fixedWireSize;
		}
	};

}