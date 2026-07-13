#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <Common/Game/InputFlags.h>
#include <Common/Packet/Game/GamePacket.h>
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

	inline constexpr std::size_t impactEffectDataWireSize
		= uint8WireSize
		+ floatWireSize
		+ floatWireSize;

	inline constexpr std::size_t playerSnapshotFixedPayloadWireSize
		= uint32WireSize
		+ int32WireSize
		+ uint32WireSize
		+ uint16WireSize;

	inline constexpr std::size_t bulletSnapshotFixedPayloadWireSize
		= uint32WireSize
		+ int32WireSize
		+ uint16WireSize
		+ uint16WireSize
		+ uint16WireSize;

	inline constexpr std::size_t impactEffectFixedPayloadWireSize
		= uint32WireSize
		+ int32WireSize
		+ uint16WireSize
		+ uint16WireSize
		+ uint16WireSize;

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

	inline void WriteEffectType(PacketWriter& writer, EffectType effectType)
	{
		writer.WriteUInt8(static_cast<std::uint8_t>(effectType));
	}

	[[nodiscard]] inline bool ReadEffectType(PacketReader& reader, EffectType& effectType) noexcept
	{
		std::uint8_t rawValue = 0;
		if (!reader.ReadUInt8(rawValue))
		{
			return false;
		}

		effectType = static_cast<EffectType>(rawValue);

		return true;
	}

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

	inline void WriteImpactEffectData(PacketWriter& writer, const ImpactEffectData& impactEffectData)
	{
		WriteEffectType(writer, impactEffectData.effectType);
		writer.WriteFloat(impactEffectData.x);
		writer.WriteFloat(impactEffectData.y);
	}

	[[nodiscard]] inline bool ReadImpactEffectData(PacketReader& reader, ImpactEffectData& impactEffectData) noexcept
	{
		return ReadEffectType(reader, impactEffectData.effectType)
			&& reader.ReadFloat(impactEffectData.x)
			&& reader.ReadFloat(impactEffectData.y);
	}

	template <>
	struct PacketCodec<JoinRequestPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::JoinRequest;
		static inline constexpr int fixedWireSize = static_cast<int>(serializedPacketHeaderSize);

	public:
		static void WritePayload(PacketWriter&, const JoinRequestPacket&)
		{}

		[[nodiscard]] static bool ReadPayload(PacketReader&, JoinRequestPacket&) noexcept
		{
			return true;
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
				|| !reader.ReadUInt16(packet.playerCount))
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

	template <>
	struct PacketCodec<ImpactEffectPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::ImpactEffect;
		static inline constexpr int fixedWireSize = 0;

	public:
		static void WritePayload(PacketWriter& writer, const ImpactEffectPacket& packet)
		{
			const std::size_t effectCount = ClampCount(packet);

			writer.WriteUInt32(packet.serverTick);
			writer.WriteInt32(packet.roomId);
			writer.WriteUInt16(packet.chunkIndex);
			writer.WriteUInt16(packet.chunkCount);
			writer.WriteUInt16(static_cast<std::uint16_t>(effectCount));

			for (std::size_t index = 0; index < effectCount; ++index)
			{
				WriteImpactEffectData(writer, packet.effects[index]);
			}
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, ImpactEffectPacket& packet) noexcept
		{
			if (!reader.ReadUInt32(packet.serverTick)
				|| !reader.ReadInt32(packet.roomId)
				|| !reader.ReadUInt16(packet.chunkIndex)
				|| !reader.ReadUInt16(packet.chunkCount)
				|| !reader.ReadUInt16(packet.effectCount))
			{
				return false;
			}

			if (packet.effectCount > maxImpactEffectsPerPacket)
			{
				return false;
			}

			for (std::size_t index = 0; index < packet.effectCount; ++index)
			{
				if (!ReadImpactEffectData(reader, packet.effects[index]))
				{
					return false;
				}
			}

			return true;
		}

	public:
		[[nodiscard]] static std::size_t ClampCount(
			const ImpactEffectPacket& packet
		) noexcept
		{
			return std::min(
				static_cast<std::size_t>(packet.effectCount),
				maxImpactEffectsPerPacket
			);
		}

		[[nodiscard]] static std::size_t GetSerializedSize(
			const ImpactEffectPacket& packet
		) noexcept
		{
			return serializedPacketHeaderSize
				+ impactEffectFixedPayloadWireSize
				+ (impactEffectDataWireSize * ClampCount(packet));
		}
	};
}