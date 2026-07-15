#pragma once

#include <cstdint>

#include <Common/Game/InputFlags.h>
#include <Common/Packet/Game/EffectPacket.h>
#include <Common/Packet/Game/SnapshotPacket.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/Serialization/PacketWireValidation.h>

namespace common::packet
{
	[[nodiscard]] consteval bool ValidatePacketWireTypes()
	{
		static_assert(isPacketBaseWireTypeValid);

		static_assert(sizeof(EffectType) == uint8WireSize);
		static_assert(sizeof(game::InputFlags) == uint8WireSize);

		return true;
	}

	inline constexpr bool isPacketWireTypeValid = ValidatePacketWireTypes();

	inline constexpr bool isPlayerStateDataValid = ValidatePacketData<PlayerStateData>();
	inline constexpr bool isBulletStateDataValid = ValidatePacketData<BulletStateData>();
	inline constexpr bool isImpactEffectDataValid = ValidatePacketData<ImpactEffectData>();
}