#pragma once

#include <Common/Packet/Account/AccountPacket.h>

#include <Server/Account/AccountService.h>

namespace server::net
{
	[[nodiscard]] common::packet::AccountLoginResponsePacket BuildAccountLoginResponse(
		common::packet::AccountLoginRequestId requestId, 
		account::LoginAccountResult loginResult
	);
}