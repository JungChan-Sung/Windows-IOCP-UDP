#include "AccountPacketMapper.h"

#include <utility>
#include <variant>

namespace server::net
{
	common::packet::AccountLoginResponsePacket BuildAccountLoginResponse(account::LoginAccountResult loginResult)
	{
		if (loginResult.has_value())
		{
			account::AccountLoginRecord accountLoginRecord = std::move(*loginResult);

			return common::packet::AccountLoginResponsePacket{
				.status = common::packet::AccountLoginResponseStatus::Succeeded,
				.accountId = accountLoginRecord.accountId,
				.nickname = std::move(accountLoginRecord.nickname),
			};
		}

		const account::LoginAccountError& loginError = loginResult.error();

		common::packet::AccountLoginResponsePacket response{};

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

		response.status = common::packet::AccountLoginResponseStatus::ServerError;
		return response;
	}
}