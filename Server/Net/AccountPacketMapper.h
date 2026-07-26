#pragma once

#include <Common/Packet/Account/AccountPacket.h>

#include <Server/Account/AccountService.h>

namespace server::net
{
	[[nodiscard]] common::packet::AccountLoginResponsePacket BuildAccountLoginResponse(account::LoginAccountResult loginResult);
}