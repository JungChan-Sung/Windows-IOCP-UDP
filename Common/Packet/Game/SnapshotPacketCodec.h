#pragma once

#include <algorithm>
#include <cstddef>

#include <Common/Packet/Game/GamePacketConstants.h>
#include <Common/Packet/Game/SnapshotPacket.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/Serialization/PacketCodec.h>
#include <Common/Packet/Serialization/PacketReader.h>
#include <Common/Packet/Serialization/PacketWriter.h>

namespace common::packet
{
	inline constexpr std::size_t playerStateDataWireSize
		= uint32WireSize
		+ floatWireSize
		+ floatWireSize
		+ int32WireSize
		+ uint8WireSize
		+ floatWireSize
		+ floatWireSize
		+ floatWireSize
		+ uint32WireSize
		+ uint32WireSize;

	inline constexpr std::size_t bulletStateDataWireSize
		= uint32WireSize
		+ floatWireSize
		+ floatWireSize;

	inline constexpr std::size_t playerSnapshotFixedPayloadWireSize
		= uint32WireSize
		+ int32WireSize
		+ uint32WireSize
		+ uint16WireSize
		+ uint32WireSize;

	inline constexpr std::size_t bulletSnapshotFixedPayloadWireSize
		= uint32WireSize
		+ int32WireSize
		+ uint16WireSize
		+ uint16WireSize
		+ uint16WireSize;

	inline void WritePlayerStateData(PacketWriter& writer, const PlayerStateData& playerStateData)
	{
		writer.WriteUInt32(playerStateData.playerId);
		writer.WriteFloat(playerStateData.x);
		writer.WriteFloat(playerStateData.y);
		writer.WriteInt32(playerStateData.hp);
		writer.WriteUInt8(playerStateData.isDead != 0 ? 1 : 0);
		writer.WriteFloat(playerStateData.respawnRemainingSeconds);
		writer.WriteFloat(playerStateData.invincibilityRemainingSeconds);
		writer.WriteFloat(playerStateData.hitFlashRemainingSeconds);
		writer.WriteUInt32(playerStateData.killCount);
		writer.WriteUInt32(playerStateData.deathCount);
	}

	[[nodiscard]] inline bool ReadPlayerStateData(PacketReader& reader, PlayerStateData& playerStateData) noexcept
	{
		return reader.ReadUInt32(playerStateData.playerId)
			&& reader.ReadFloat(playerStateData.x)
			&& reader.ReadFloat(playerStateData.y)
			&& reader.ReadInt32(playerStateData.hp)
			&& reader.ReadUInt8(playerStateData.isDead)
			&& reader.ReadFloat(playerStateData.respawnRemainingSeconds)
			&& reader.ReadFloat(playerStateData.invincibilityRemainingSeconds)
			&& reader.ReadFloat(playerStateData.hitFlashRemainingSeconds)
			&& reader.ReadUInt32(playerStateData.killCount)
			&& reader.ReadUInt32(playerStateData.deathCount);
	}

	inline void WriteBulletStateData(PacketWriter& writer, const BulletStateData& bulletStateData)
	{
		writer.WriteUInt32(bulletStateData.bulletId);
		writer.WriteFloat(bulletStateData.x);
		writer.WriteFloat(bulletStateData.y);
	}

	[[nodiscard]] inline bool ReadBulletStateData(PacketReader& reader, BulletStateData& bulletStateData) noexcept
	{
		return reader.ReadUInt32(bulletStateData.bulletId)
			&& reader.ReadFloat(bulletStateData.x)
			&& reader.ReadFloat(bulletStateData.y);
	}

	template <>
	struct PacketCodec<PlayerSnapshotPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::PlayerSnapshot;
		static inline constexpr int fixedWireSize = 0;

	public:
		static void WritePayload(PacketWriter& writer, const PlayerSnapshotPacket& packet)
		{
			const std::size_t playerCount = ClampCount(packet);

			writer.WriteUInt32(packet.serverTick);
			writer.WriteInt32(packet.roomId);
			writer.WriteUInt32(packet.lastProcessedInputSequence);
			writer.WriteUInt16(static_cast<std::uint16_t>(playerCount));
			writer.WriteInt32(packet.serverTickIntervalMilliseconds);

			for (std::size_t index = 0; index < playerCount; ++index)
			{
				WritePlayerStateData(writer, packet.players[index]);
			}
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, PlayerSnapshotPacket& packet) noexcept
		{
			if (!reader.ReadUInt32(packet.serverTick)
				|| !reader.ReadInt32(packet.roomId)
				|| !reader.ReadUInt32(packet.lastProcessedInputSequence)
				|| !reader.ReadUInt16(packet.playerCount)
				|| !reader.ReadUInt32(packet.serverTickIntervalMilliseconds))
			{
				return false;
			}

			if (packet.playerCount > maxPlayersPerSnapshot)
			{
				return false;
			}

			for (std::size_t index = 0; index < packet.playerCount; ++index)
			{
				if (!ReadPlayerStateData(reader, packet.players[index]))
				{
					return false;
				}
			}

			return true;
		}

	public:
		[[nodiscard]] static std::size_t ClampCount(const PlayerSnapshotPacket& packet) noexcept
		{
			return std::min(static_cast<std::size_t>(packet.playerCount), maxPlayersPerSnapshot);
		}

		[[nodiscard]] static std::size_t GetSerializedSize(const PlayerSnapshotPacket& packet) noexcept
		{
			return serializedPacketHeaderSize
				+ playerSnapshotFixedPayloadWireSize
				+ (playerStateDataWireSize * ClampCount(packet));
		}
	};

	template <>
	struct PacketCodec<BulletSnapshotPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::BulletSnapshot;
		static inline constexpr int fixedWireSize = 0;

	public:
		static void WritePayload(PacketWriter& writer, const BulletSnapshotPacket& packet)
		{
			const std::size_t bulletCount = ClampCount(packet);

			writer.WriteUInt32(packet.serverTick);
			writer.WriteInt32(packet.roomId);
			writer.WriteUInt16(packet.chunkIndex);
			writer.WriteUInt16(packet.chunkCount);
			writer.WriteUInt16(static_cast<std::uint16_t>(bulletCount));

			for (std::size_t index = 0; index < bulletCount; ++index)
			{
				WriteBulletStateData(writer, packet.bullets[index]);
			}
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, BulletSnapshotPacket& packet) noexcept
		{
			if (!reader.ReadUInt32(packet.serverTick)
				|| !reader.ReadInt32(packet.roomId)
				|| !reader.ReadUInt16(packet.chunkIndex)
				|| !reader.ReadUInt16(packet.chunkCount)
				|| !reader.ReadUInt16(packet.bulletCount))
			{
				return false;
			}

			if (packet.bulletCount > maxBulletsPerSnapshot)
			{
				return false;
			}

			for (std::size_t index = 0; index < packet.bulletCount; ++index)
			{
				if (!ReadBulletStateData(reader, packet.bullets[index]))
				{
					return false;
				}
			}

			return true;
		}

	public:
		[[nodiscard]] static std::size_t ClampCount(const BulletSnapshotPacket& packet) noexcept
		{
			return std::min(static_cast<std::size_t>(packet.bulletCount), maxBulletsPerSnapshot);
		}

		[[nodiscard]] static std::size_t GetSerializedSize(const BulletSnapshotPacket& packet) noexcept
		{
			return serializedPacketHeaderSize
				+ bulletSnapshotFixedPayloadWireSize
				+ (bulletStateDataWireSize * ClampCount(packet));
		}
	};
}