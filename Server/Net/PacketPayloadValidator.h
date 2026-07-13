#pragma once

#include <Common/Packet/Game/GamePacket.h>

namespace server::net
{
	class PacketPayloadValidator
	{
	public:
		enum class PayloadValidationStatus
		{
			Succeeded = 0,
			InvalidInputSequence,
			InvalidInputFlags,
			InvalidRoomId
		};

	public:
		PacketPayloadValidator() = delete;
		~PacketPayloadValidator() = delete;

		PacketPayloadValidator(const PacketPayloadValidator&) = delete;
		PacketPayloadValidator& operator=(const PacketPayloadValidator&) = delete;

		PacketPayloadValidator(PacketPayloadValidator&&) = delete;
		PacketPayloadValidator& operator=(PacketPayloadValidator&&) = delete;

	public:
		[[nodiscard]] static PayloadValidationStatus ValidateInputCommandPacket(
			const common::packet::InputCommandPacket& packet
		) noexcept;
		[[nodiscard]] static PayloadValidationStatus ValidateJoinRoomRequestPacket(
			const common::packet::JoinRoomRequestPacket& packet
		) noexcept;

		[[nodiscard]] static const char* ToString(PayloadValidationStatus status) noexcept;
	};
}