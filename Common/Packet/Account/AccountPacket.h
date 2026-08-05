#pragma once

#include <cstdint>
#include <string>

#include <Common/Net/SessionToken.h>
#include <Common/Packet/PacketHeader.h>

namespace common::packet
{
	using AccountLoginRequestId = std::uint64_t;

	inline constexpr AccountLoginRequestId invalidAccountLoginRequestId = 0;

	enum class AccountLoginResponseStatus : std::uint8_t
	{
		Succeeded,
		InvalidRequest,
		InvalidCredentials,
		ServerError,
		AlreadyLoggedIn,
	};

	struct AccountLoginRequestPacket
	{
	public:
		PacketHeader header{ 0, PacketType::AccountLoginRequest };

		AccountLoginRequestId requestId = invalidAccountLoginRequestId;
		std::string loginName;
		std::string passwordHash;
	};

	struct AccountLoginResponsePacket
	{
	public:
		PacketHeader header{ 0, PacketType::AccountLoginResponse };

		AccountLoginRequestId requestId = invalidAccountLoginRequestId;
		AccountLoginResponseStatus status = AccountLoginResponseStatus::ServerError;

		std::int64_t accountId = 0;
		common::net::SessionToken sessionToken{};
		std::string nickname;
	};
}