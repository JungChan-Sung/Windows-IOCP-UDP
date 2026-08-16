#pragma once

#include <Common/Packet/Account/AccountPacket.h>

#include <Server/Account/AccountService.h>
#include <Server/Service/AccountLoginAdmissionService.h>

namespace server::protocol
{
	[[nodiscard]] common::packet::AccountLoginResponsePacket BuildAccountLoginResponse(
		common::packet::AccountLoginRequestId requestId, 
		account::LoginAccountResult loginResult
	);

	void ApplyAccountLoginAdmissionResult(
		const service::AccountLoginAdmissionService::Result&
		admissionResult,
		common::packet::AccountLoginResponsePacket& responsePacket
	) noexcept;
}