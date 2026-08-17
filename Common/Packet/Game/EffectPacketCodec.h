#pragma once

#include <algorithm>
#include <cstddef>

#include <Common/Packet/Game/EffectPacket.h>
#include <Common/Packet/Game/GamePacketConstants.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/Serialization/PacketCodec.h>
#include <Common/Packet/Serialization/PacketReader.h>
#include <Common/Packet/Serialization/PacketWriter.h>

namespace common::packet
{
	inline constexpr std::size_t impactEffectDataWireSize
		= uint8WireSize
		+ floatWireSize
		+ floatWireSize;

	inline constexpr std::size_t impactEffectFixedPayloadWireSize
		= uint32WireSize
		+ int32WireSize
		+ uint16WireSize
		+ uint16WireSize
		+ uint16WireSize;

	inline void WriteEffectType(PacketWriter& writer, game::EffectType effectType)
	{
		writer.WriteUInt8(static_cast<std::uint8_t>(effectType));
	}

	[[nodiscard]] inline bool ReadEffectType(PacketReader& reader, game::EffectType& effectType) noexcept
	{
		std::uint8_t rawValue = 0;
		if (!reader.ReadUInt8(rawValue))
		{
			return false;
		}

		effectType = static_cast<game::EffectType>(rawValue);

		return true;
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
			return std::min(static_cast<std::size_t>(packet.effectCount), maxImpactEffectsPerPacket);
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