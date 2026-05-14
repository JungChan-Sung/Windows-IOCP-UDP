#include "PacketPayloadValidator.h"

#include <type_traits>

#include <Common/Game/InputFlags.h>

namespace
{
	using InputFlagsUnderlyingType = std::underlying_type_t<common::game::InputFlags>;

	inline constexpr InputFlagsUnderlyingType validInputFlagsMask =
		static_cast<InputFlagsUnderlyingType>(common::game::InputFlags::Up)
		| static_cast<InputFlagsUnderlyingType>(common::game::InputFlags::Down)
		| static_cast<InputFlagsUnderlyingType>(common::game::InputFlags::Left)
		| static_cast<InputFlagsUnderlyingType>(common::game::InputFlags::Right);

	[[nodiscard]] bool IsValidInputFlags(common::game::InputFlags inputFlags) noexcept
	{
		const InputFlagsUnderlyingType rawInputFlags = static_cast<InputFlagsUnderlyingType>(inputFlags);
		return (rawInputFlags & ~validInputFlagsMask) == 0;
	}
}

namespace server::net
{
	PacketPayloadValidator::PayloadValidationStatus PacketPayloadValidator::ValidateInputCommandPacket(const common::packet::InputCommandPacket& packet) noexcept
	{
		if (packet.inputSequence == 0)
		{
			return PayloadValidationStatus::InvalidInputSequence;
		}

		if (!IsValidInputFlags(packet.inputFlags))
		{
			return PayloadValidationStatus::InvalidInputFlags;
		}

		return PayloadValidationStatus::Succeeded;
	}

	PacketPayloadValidator::PayloadValidationStatus PacketPayloadValidator::ValidateJoinRoomRequestPacket(const common::packet::JoinRoomRequestPacket& packet) noexcept
	{
		if (packet.roomId <= 0)
		{
			return PayloadValidationStatus::InvalidRoomId;
		}

		return PayloadValidationStatus::Succeeded;
	}

	const char* PacketPayloadValidator::ToString(PayloadValidationStatus status) noexcept
	{
		switch (status)
		{
		case PayloadValidationStatus::Succeeded:
			return "Succeeded";

		case PayloadValidationStatus::InvalidInputSequence:
			return "InvalidInputSequence";

		case PayloadValidationStatus::InvalidInputFlags:
			return "InvalidInputFlags";

		case PayloadValidationStatus::InvalidRoomId:
			return "InvalidRoomId";

		default:
			return "Unknown";
		}
	}
}