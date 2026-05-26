#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "GamePacket.h"

namespace common::packet
{
	using PacketBuffer = std::vector<char>;

	class PacketWriter
	{
	private:
		PacketBuffer buffer_;

	public:
		PacketWriter() = default;
		~PacketWriter() noexcept = default;

		PacketWriter(const PacketWriter&) = delete;
		PacketWriter& operator=(const PacketWriter&) = delete;

		PacketWriter(PacketWriter&&) noexcept = default;
		PacketWriter& operator=(PacketWriter&&) noexcept = default;

	public:
		void Reserve(std::size_t size)
		{
			buffer_.reserve(size);
		}

		void WriteUInt8(std::uint8_t value)
		{
			buffer_.push_back(static_cast<char>(value));
		}

		void WriteUInt16(std::uint16_t value)
		{
			WriteUInt8(static_cast<std::uint8_t>(value & 0x00FF));
			WriteUInt8(static_cast<std::uint8_t>((value >> 8) & 0x00FF));
		}

		void WriteUInt32(std::uint32_t value)
		{
			WriteUInt8(static_cast<std::uint8_t>(value & 0x000000FF));
			WriteUInt8(static_cast<std::uint8_t>((value >> 8) & 0x000000FF));
			WriteUInt8(static_cast<std::uint8_t>((value >> 16) & 0x000000FF));
			WriteUInt8(static_cast<std::uint8_t>((value >> 24) & 0x000000FF));
		}

		void WriteInt32(std::int32_t value)
		{
			WriteUInt32(std::bit_cast<std::uint32_t>(value));
		}

		void WriteFloat(float value)
		{
			WriteUInt32(std::bit_cast<std::uint32_t>(value));
		}

		void WritePacketType(PacketType packetType)
		{
			WriteUInt16(static_cast<std::uint16_t>(packetType));
		}

		void WriteInputFlags(game::InputFlags inputFlags)
		{
			WriteUInt8(static_cast<std::uint8_t>(inputFlags));
		}

		void WriteEffectType(EffectType effectType)
		{
			WriteUInt8(static_cast<std::uint8_t>(effectType));
		}

	public:
		[[nodiscard]] const PacketBuffer& GetBuffer() const noexcept
		{
			return buffer_;
		}

		[[nodiscard]] PacketBuffer TakeBuffer() noexcept
		{
			return std::move(buffer_);
		}
	};

	class PacketReader
	{
	private:
		const char* data_ = nullptr;
		int size_ = 0;
		int offset_ = 0;

	public:
		PacketReader(const char* data, int size) noexcept
			: data_(data),
			size_(size)
		{}
		~PacketReader() noexcept = default;

		PacketReader(const PacketReader&) = delete;
		PacketReader& operator=(const PacketReader&) = delete;

		PacketReader(PacketReader&&) = delete;
		PacketReader& operator=(PacketReader&&) = delete;

	public:
		[[nodiscard]] bool ReadUInt8(std::uint8_t& value) noexcept
		{
			if (RemainingSize() < static_cast<int>(uint8WireSize))
			{
				return false;
			}

			value = static_cast<std::uint8_t>(static_cast<unsigned char>(data_[offset_]));
			++offset_;
			return true;
		}

		[[nodiscard]] bool ReadUInt16(std::uint16_t& value) noexcept
		{
			std::uint8_t byte0 = 0;
			std::uint8_t byte1 = 0;

			if (!ReadUInt8(byte0) || !ReadUInt8(byte1))
			{
				return false;
			}

			value = static_cast<std::uint16_t>(
				static_cast<std::uint16_t>(byte0)
				| static_cast<std::uint16_t>(static_cast<std::uint16_t>(byte1) << 8)
				);

			return true;
		}

		[[nodiscard]] bool ReadUInt32(std::uint32_t& value) noexcept
		{
			std::uint8_t byte0 = 0;
			std::uint8_t byte1 = 0;
			std::uint8_t byte2 = 0;
			std::uint8_t byte3 = 0;

			if (!ReadUInt8(byte0) || !ReadUInt8(byte1) || !ReadUInt8(byte2) || !ReadUInt8(byte3))
			{
				return false;
			}

			value = static_cast<std::uint32_t>(byte0)
				| (static_cast<std::uint32_t>(byte1) << 8)
				| (static_cast<std::uint32_t>(byte2) << 16)
				| (static_cast<std::uint32_t>(byte3) << 24);

			return true;
		}

		[[nodiscard]] bool ReadInt32(std::int32_t& value) noexcept
		{
			std::uint32_t rawValue = 0;
			if (!ReadUInt32(rawValue))
			{
				return false;
			}

			value = std::bit_cast<std::int32_t>(rawValue);
			return true;
		}

		[[nodiscard]] bool ReadFloat(float& value) noexcept
		{
			std::uint32_t rawValue = 0;
			if (!ReadUInt32(rawValue))
			{
				return false;
			}

			value = std::bit_cast<float>(rawValue);
			return true;
		}

		[[nodiscard]] bool ReadPacketType(PacketType& packetType) noexcept
		{
			std::uint16_t rawValue = 0;
			if (!ReadUInt16(rawValue))
			{
				return false;
			}

			packetType = static_cast<PacketType>(rawValue);
			return true;
		}

		[[nodiscard]] bool ReadInputFlags(game::InputFlags& inputFlags) noexcept
		{
			std::uint8_t rawValue = 0;
			if (!ReadUInt8(rawValue))
			{
				return false;
			}

			inputFlags = static_cast<game::InputFlags>(rawValue);
			return true;
		}

		[[nodiscard]] bool ReadEffectType(EffectType& effectType) noexcept
		{
			std::uint8_t rawValue = 0;
			if (!ReadUInt8(rawValue))
			{
				return false;
			}

			effectType = static_cast<EffectType>(rawValue);
			return true;
		}

	public:
		[[nodiscard]] int RemainingSize() const noexcept
		{
			return size_ - offset_;
		}

		[[nodiscard]] bool IsComplete() const noexcept
		{
			return offset_ == size_;
		}
	};

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

	[[nodiscard]] inline std::uint16_t MakePacketHeaderVersion(bool isReliable) noexcept
	{
		if (isReliable)
		{
			return protocolVersion | packetHeaderReliableFlag;
		}

		return protocolVersion;
	}

	[[nodiscard]] inline bool IsReliablePacketHeader(const PacketHeader& packetHeader) noexcept
	{
		return (packetHeader.version & packetHeaderReliableFlag) != 0;
	}

	[[nodiscard]] inline std::uint16_t GetPacketHeaderProtocolVersion(const PacketHeader& packetHeader) noexcept
	{
		return packetHeader.version & packetHeaderVersionMask;
	}

	inline void WritePacketHeader(PacketWriter& writer, std::uint16_t packetSize, PacketType packetType, bool isReliable = false)
	{
		writer.WriteUInt16(packetSize);
		writer.WritePacketType(packetType);
		writer.WriteUInt16(MakePacketHeaderVersion(isReliable));
	}

	[[nodiscard]] inline bool ReadPacketHeader(PacketReader& reader, PacketHeader& packetHeader) noexcept
	{
		if (!reader.ReadUInt16(packetHeader.size))
		{
			return false;
		}

		if (!reader.ReadPacketType(packetHeader.type))
		{
			return false;
		}

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
		writer.WriteEffectType(impactEffectData.effectType);
		writer.WriteFloat(impactEffectData.x);
		writer.WriteFloat(impactEffectData.y);
	}

	[[nodiscard]] inline bool ReadImpactEffectData(PacketReader& reader, ImpactEffectData& impactEffectData) noexcept
	{
		return reader.ReadEffectType(impactEffectData.effectType)
			&& reader.ReadFloat(impactEffectData.x)
			&& reader.ReadFloat(impactEffectData.y);
	}

	[[nodiscard]] inline std::optional<PacketBuffer> FinishSerializedPacket(
		PacketWriter& writer,
		std::size_t expectedSize
	)
	{
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

		if (IsReliablePacketHeader(packetHeader))
		{
			return false;
		}

		return true;
	}

	template <typename TPacket>
	struct PacketCodec;

	template <typename TPacket>
	inline constexpr int packetExpectedSize = PacketCodec<TPacket>::fixedWireSize;

	template <>
	struct PacketCodec<JoinRequestPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::JoinRequest;
		static inline constexpr int fixedWireSize = static_cast<int>(serializedPacketHeaderSize);

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const JoinRequestPacket&) noexcept
		{
			return fixedWireSize;
		}

		static void WritePayload(PacketWriter&, const JoinRequestPacket&)
		{}

		[[nodiscard]] static bool ReadPayload(PacketReader&, JoinRequestPacket&) noexcept
		{
			return true;
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
			+ floatWireSize
			+ floatWireSize
			);

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const JoinResponsePacket&) noexcept
		{
			return fixedWireSize;
		}

		static void WritePayload(PacketWriter& writer, const JoinResponsePacket& packet)
		{
			writer.WriteUInt32(packet.playerId);
			writer.WriteFloat(packet.spawnX);
			writer.WriteFloat(packet.spawnY);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, JoinResponsePacket& packet) noexcept
		{
			return reader.ReadUInt32(packet.playerId)
				&& reader.ReadFloat(packet.spawnX)
				&& reader.ReadFloat(packet.spawnY);
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
		[[nodiscard]] static std::size_t GetSerializedSize(const InputCommandPacket&) noexcept
		{
			return fixedWireSize;
		}

		static void WritePayload(PacketWriter& writer, const InputCommandPacket& packet)
		{
			writer.WriteUInt32(packet.inputSequence);
			writer.WriteInputFlags(packet.inputFlags);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, InputCommandPacket& packet) noexcept
		{
			return reader.ReadUInt32(packet.inputSequence)
				&& reader.ReadInputFlags(packet.inputFlags);
		}
	};

	template <>
	struct PacketCodec<FireRequestPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::FireRequest;
		static inline constexpr int fixedWireSize = static_cast<int>(serializedPacketHeaderSize);

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const FireRequestPacket&) noexcept
		{
			return fixedWireSize;
		}

		static void WritePayload(PacketWriter&, const FireRequestPacket&)
		{}

		[[nodiscard]] static bool ReadPayload(PacketReader&, FireRequestPacket&) noexcept
		{
			return true;
		}
	};

	template <>
	struct PacketCodec<LeaveRequestPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::LeaveRequest;
		static inline constexpr int fixedWireSize = static_cast<int>(serializedPacketHeaderSize);

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const LeaveRequestPacket&) noexcept
		{
			return fixedWireSize;
		}

		static void WritePayload(PacketWriter&, const LeaveRequestPacket&)
		{}

		[[nodiscard]] static bool ReadPayload(PacketReader&, LeaveRequestPacket&) noexcept
		{
			return true;
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
		[[nodiscard]] static std::size_t GetSerializedSize(const JoinRoomRequestPacket&) noexcept
		{
			return fixedWireSize;
		}

		static void WritePayload(PacketWriter& writer, const JoinRoomRequestPacket& packet)
		{
			writer.WriteInt32(packet.roomId);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, JoinRoomRequestPacket& packet) noexcept
		{
			return reader.ReadInt32(packet.roomId);
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
		[[nodiscard]] static std::size_t GetSerializedSize(const JoinRoomResponsePacket&) noexcept
		{
			return fixedWireSize;
		}

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
		[[nodiscard]] static std::size_t GetSerializedSize(const PlayerJoinedPacket&) noexcept
		{
			return fixedWireSize;
		}

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
		[[nodiscard]] static std::size_t GetSerializedSize(const PlayerLeftPacket&) noexcept
		{
			return fixedWireSize;
		}

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
	};

	template <>
	struct PacketCodec<PlayerSnapshotPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::PlayerSnapshot;
		static inline constexpr int fixedWireSize = 0;

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
	};

	template <>
	struct PacketCodec<BulletSnapshotPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::BulletSnapshot;
		static inline constexpr int fixedWireSize = 0;

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
	};

	template <>
	struct PacketCodec<ImpactEffectPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::ImpactEffect;
		static inline constexpr int fixedWireSize = 0;

	public:
		[[nodiscard]] static std::size_t ClampCount(const ImpactEffectPacket& packet) noexcept
		{
			return std::min(static_cast<std::size_t>(packet.effectCount), maxImpactEffectsPerPacket);
		}

		[[nodiscard]] static std::size_t GetSerializedSize(const ImpactEffectPacket& packet) noexcept
		{
			return serializedPacketHeaderSize
				+ impactEffectFixedPayloadWireSize
				+ (impactEffectDataWireSize * ClampCount(packet));
		}

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
	};

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

		WritePacketHeader(
			writer,
			static_cast<std::uint16_t>(serializedSize),
			PacketCodec<TPacket>::packetType
		);

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