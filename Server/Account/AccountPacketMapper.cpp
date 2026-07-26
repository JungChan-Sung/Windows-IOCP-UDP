#include "AccountPacketMapper.h"

#include <utility>
#include <variant>

namespace server::account
{
	common::packet::AccountLoginResponsePacket BuildAccountLoginResponse(LoginAccountResult loginResult)
	{
		if (loginResult.has_value())
		{
			AccountLoginRecord accountLoginRecord = std::move(*loginResult);

			return common::packet::AccountLoginResponsePacket{
				.status = common::packet::AccountLoginResponseStatus::Succeeded,
				.accountId = accountLoginRecord.accountId,
				.nickname = std::move(accountLoginRecord.nickname),
			};
		}

		const LoginAccountError& loginError = loginResult.error();

		common::packet::AccountLoginResponsePacket response{};

		if (std::holds_alternative<persistence::account::AccountValidationError>(loginError))
		{
			response.status = common::packet::AccountLoginResponseStatus::InvalidRequest;
			return response;
		}

		if (std::holds_alternative<LoginAccountFailure>(loginError))
		{
			response.status = common::packet::AccountLoginResponseStatus::InvalidCredentials;
			return response;
		}

		response.status = common::packet::AccountLoginResponseStatus::ServerError;
		return response;
	}
}