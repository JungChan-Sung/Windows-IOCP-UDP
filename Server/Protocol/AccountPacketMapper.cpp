#include "AccountPacketMapper.h"

#include <utility>
#include <variant>

namespace
{
	void ClearAccountLoginResponseData(common::packet::AccountLoginResponsePacket& responsePacket) noexcept
	{
		responsePacket.accountId = 0;
		responsePacket.sessionToken = common::net::invalidSessionToken;
		responsePacket.nickname.clear();
	}
}

namespace server::protocol
{
	common::packet::AccountLoginResponsePacket BuildAccountLoginResponse(
		common::packet::AccountLoginRequestId requestId,
		const account::LoginAccountResult& loginResult
	)
	{
		if (loginResult.has_value())
		{
			const account::AccountLoginRecord& accountLoginRecord = *loginResult;

			return common::packet::AccountLoginResponsePacket{
				.requestId = requestId,
				.status = common::packet::AccountLoginResponseStatus::Succeeded,
				.accountId = accountLoginRecord.accountId,
				.nickname = accountLoginRecord.nickname,
			};
		}

		const account::LoginAccountError& loginError = loginResult.error();

		common::packet::AccountLoginResponsePacket response{};
		response.requestId = requestId;

		if (std::holds_alternative<persistence::account::AccountValidationError>(loginError))
		{
			response.status = common::packet::AccountLoginResponseStatus::InvalidRequest;
			return response;
		}

		if (std::holds_alternative<account::LoginAccountFailure>(loginError))
		{
			response.status = common::packet::AccountLoginResponseStatus::InvalidCredentials;
			return response;
		}

		return BuildAccountLoginServerErrorResponse(requestId);
	}

	common::packet::AccountLoginResponsePacket BuildAccountLoginServerErrorResponse(common::packet::AccountLoginRequestId requestId)
	{
		common::packet::AccountLoginResponsePacket response{};
		response.requestId = requestId;
		response.status = common::packet::AccountLoginResponseStatus::ServerError;

		ClearAccountLoginResponseData(response);

		return response;
	}

	void ApplyAccountLoginAdmissionResult(
		const service::AccountLoginAdmissionService::Result&
		admissionResult,
		common::packet::AccountLoginResponsePacket& responsePacket
	) noexcept
	{
		using AdmissionStatus = service::AccountLoginAdmissionService::Status;

		switch (admissionResult.status)
		{
		case AdmissionStatus::Authenticated:
		case AdmissionStatus::ExistingSession:
			responsePacket.status = common::packet::AccountLoginResponseStatus::Succeeded;
			responsePacket.sessionToken = admissionResult.sessionToken;
			return;

		case AdmissionStatus::AlreadyLoggedIn:
			responsePacket.status = common::packet::AccountLoginResponseStatus::AlreadyLoggedIn;
			break;

		case AdmissionStatus::TokenGenerationFailed:
		case AdmissionStatus::RegistrationFailed:
			responsePacket.status = common::packet::AccountLoginResponseStatus::ServerError;
			break;
		}

		ClearAccountLoginResponseData(responsePacket);
	}
}