#pragma once

#include <cstddef>
#include <cstdint>

#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/Serialization/PacketCodec.h>
#include <Common/Packet/Serialization/PacketReader.h>
#include <Common/Packet/Serialization/PacketWriter.h>

namespace common::packet
{
	[[nodiscard]] inline bool IsValidAccountLoginResponseStatus(AccountLoginResponseStatus status) noexcept
	{
		switch (status)
		{
		case AccountLoginResponseStatus::Succeeded:
		case AccountLoginResponseStatus::InvalidRequest:
		case AccountLoginResponseStatus::InvalidCredentials:
		case AccountLoginResponseStatus::ServerError:
			return true;

		default:
			return false;
		}
	}

	template <>
	struct PacketCodec<AccountLoginRequestPacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::AccountLoginRequest;
		static inline constexpr int fixedWireSize = 0;

	public:
		static void WritePayload(PacketWriter& writer, const AccountLoginRequestPacket& packet)
		{
			writer.WriteUInt64(packet.requestId);
			writer.WriteString(packet.loginName);
			writer.WriteString(packet.passwordHash);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, AccountLoginRequestPacket& packet)
		{
			return reader.ReadUInt64(packet.requestId)
				&& reader.ReadString(packet.loginName)
				&& reader.ReadString(packet.passwordHash);
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const AccountLoginRequestPacket& packet) noexcept
		{
			return serializedPacketHeaderSize
				+ uint64WireSize
				+ GetSerializedStringSize(packet.loginName)
				+ GetSerializedStringSize(packet.passwordHash);
		}
	};

	template <>
	struct PacketCodec<AccountLoginResponsePacket>
	{
	public:
		static inline constexpr PacketType packetType = PacketType::AccountLoginResponse;
		static inline constexpr int fixedWireSize = 0;

	public:
		static void WritePayload(PacketWriter& writer, const AccountLoginResponsePacket& packet)
		{
			writer.WriteUInt64(packet.requestId);
			writer.WriteUInt8(static_cast<std::uint8_t>(packet.status));
			writer.WriteInt64(packet.accountId);
			writer.WriteString(packet.nickname);
		}

		[[nodiscard]] static bool ReadPayload(PacketReader& reader, AccountLoginResponsePacket& packet)
		{
			if (!reader.ReadUInt64(packet.requestId))
			{
				return false;
			}

			std::uint8_t rawStatus = 0;
			if (!reader.ReadUInt8(rawStatus))
			{
				return false;
			}

			packet.status = static_cast<AccountLoginResponseStatus>(rawStatus);
			if (!IsValidAccountLoginResponseStatus(packet.status))
			{
				return false;
			}

			return reader.ReadInt64(packet.accountId)
				&& reader.ReadString(packet.nickname);
		}

	public:
		[[nodiscard]] static std::size_t GetSerializedSize(const AccountLoginResponsePacket& packet) noexcept
		{
			return serializedPacketHeaderSize
				+ uint64WireSize
				+ uint8WireSize
				+ int64WireSize
				+ GetSerializedStringSize(packet.nickname);
		}
	};
}