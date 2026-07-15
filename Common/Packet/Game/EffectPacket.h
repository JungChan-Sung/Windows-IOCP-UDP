#pragma once

#include <array>
#include <cstdint>

#include <Common/Packet/Game/GamePacketConstants.h>
#include <Common/Packet/PacketHeader.h>

namespace common::packet
{
	enum class EffectType : std::uint8_t
	{
		None = 0,
		Impact = 1,
		Spawn = 2,
	};

	struct ImpactEffectData
	{
	public:
		EffectType effectType = EffectType::None;
		float x = 0.0F;
		float y = 0.0F;
	};

	struct ImpactEffectPacket
	{
	public:
		PacketHeader header{ 0, PacketType::ImpactEffect };
		std::uint32_t serverTick = 0;
		std::int32_t roomId = 0;

		std::uint16_t chunkIndex = 0;
		std::uint16_t chunkCount = 1;

		std::uint16_t effectCount = 0;
		std::array<ImpactEffectData, maxImpactEffectsPerPacket> effects{};
	};
}