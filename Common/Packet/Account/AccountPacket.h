#pragma once

#include <cstdint>
#include <string>

#include <Common/Packet/PacketHeader.h>

namespace common::packet
{
	enum class AccountLoginResponseStatus : std::uint8_t
	{
		Succeeded,
		InvalidRequest,
		InvalidCredentials,
		ServerError,
	};

	struct AccountLoginRequestPacket
	{
	public:
		PacketHeader header{ 0, PacketType::AccountLoginRequest };

		std::string loginName;
		std::string passwordHash;
	};

	struct AccountLoginResponsePacket
	{
	public:
		PacketHeader header{ 0, PacketType::AccountLoginResponse };

		AccountLoginResponseStatus status = AccountLoginResponseStatus::ServerError;
		std::int64_t accountId = 0;
		std::string nickname;
	};
}