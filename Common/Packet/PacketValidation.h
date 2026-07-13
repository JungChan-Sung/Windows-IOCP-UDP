#pragma once

#include <cstdint>
#include <type_traits>

#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/PacketHeader.h>
#include <Common/Packet/PacketType.h>

namespace common::packet
{

	[[nodiscard]] consteval bool ValidatePacketWireTypes()
	{
		static_assert(sizeof(std::uint8_t) == uint8WireSize);
		static_assert(sizeof(std::uint16_t) == uint16WireSize);
		static_assert(sizeof(std::uint32_t) == uint32WireSize);
		static_assert(sizeof(std::int32_t) == int32WireSize);
		static_assert(sizeof(float) == floatWireSize);

		static_assert(sizeof(PacketType) == packetTypeWireSize);
		static_assert(sizeof(EffectType) == uint8WireSize);
		static_assert(sizeof(game::InputFlags) == uint8WireSize);

		return true;
	}

	template <typename TData>
	[[nodiscard]] consteval bool ValidatePacketData()
	{
		using Data = std::remove_cvref_t<TData>;

		static_assert(std::is_standard_layout_v<Data>);
		static_assert(std::is_trivially_copyable_v<Data>);

		return true;
	}

	inline constexpr bool isPacketWireTypeValid = ValidatePacketWireTypes();

	inline constexpr bool isPlayerStateDataValid = ValidatePacketData<PlayerStateData>();
	inline constexpr bool isBulletStateDataValid = ValidatePacketData<BulletStateData>();
	inline constexpr bool isImpactEffectDataValid = ValidatePacketData<ImpactEffectData>();
}