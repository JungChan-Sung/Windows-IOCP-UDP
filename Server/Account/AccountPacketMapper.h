#pragma once

#include <Common/Packet/Account/AccountPacket.h>

#include <Server/Account/AccountService.h>

namespace server::account
{
	[[nodiscard]] common::packet::AccountLoginResponsePacket BuildAccountLoginResponse(LoginAccountResult loginResult);
}