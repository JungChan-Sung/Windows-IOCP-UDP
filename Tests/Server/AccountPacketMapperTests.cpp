#include "AccountPacketMapperTests.h"

#include <string>
#include <string_view>
#include <utility>

#include <Common/Packet/Account/AccountPacket.h>

#include <Persistence/Account/AccountValidation.h>
#include <Persistence/Core/DatabaseError.h>

#include <Server/Account/AccountService.h>
#include <Server/Net/AccountPacketMapper.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void ExpectFailureResponse(
		tests::DebugTestResult& result,
		const common::packet::AccountLoginResponsePacket& response,
		common::packet::AccountLoginResponseStatus expectedStatus,
		std::string_view testName
	)
	{
		tests::Expect(
			result,
			response.status == expectedStatus,
			std::string(testName) + ": status"
		);

		tests::Expect(
			result,
			response.accountId == 0 && response.nickname.empty(),
			std::string(testName) + ": clear account data"
		);
	}
}

namespace tests::server
{
	DebugTestResult RunAccountPacketMapperTests()
	{
		DebugTestResult result{};

		{
			::server::account::LoginAccountResult loginResult
				= ::server::account::AccountLoginRecord{
					.accountId = 1001,
					.loginName = "account",
					.nickname = "nickname",
			};

			const common::packet::AccountLoginResponsePacket response
				= ::server::net::BuildAccountLoginResponse(
					std::move(loginResult)
				);

			tests::Expect(
				result,
				response.status
				== common::packet::AccountLoginResponseStatus::Succeeded,
				"AccountPacketMapper: success status"
			);

			tests::Expect(
				result,
				response.accountId == 1001,
				"AccountPacketMapper: success account id"
			);

			tests::Expect(
				result,
				response.nickname == "nickname",
				"AccountPacketMapper: success nickname"
			);
		}

		{
			::server::account::LoginAccountResult loginResult = std::unexpected(
				::server::account::LoginAccountError{
					persistence::account::AccountValidationError{
						.field = persistence::account::AccountField::LoginName,
						.failure = persistence::account::AccountValidationFailure::Empty,
					}
				}
			);

			const common::packet::AccountLoginResponsePacket response
				= ::server::net::BuildAccountLoginResponse(
					std::move(loginResult)
				);

			ExpectFailureResponse(
				result,
				response,
				common::packet::AccountLoginResponseStatus::InvalidRequest,
				"AccountPacketMapper: validation failure"
			);
		}

		{
			::server::account::LoginAccountResult loginResult = std::unexpected(
				::server::account::LoginAccountError{
					::server::account::LoginAccountFailure::InvalidCredentials
				}
			);

			const common::packet::AccountLoginResponsePacket response
				= ::server::net::BuildAccountLoginResponse(
					std::move(loginResult)
				);

			ExpectFailureResponse(
				result,
				response,
				common::packet::AccountLoginResponseStatus::InvalidCredentials,
				"AccountPacketMapper: invalid credentials"
			);
		}

		{
			::server::account::LoginAccountResult loginResult = std::unexpected(
				::server::account::LoginAccountError{
					persistence::core::DatabaseError{
						.failure = persistence::core::DatabaseFailure::ConnectionOpenFailed,
						.message = "Database unavailable.",
					}
				}
			);

			const common::packet::AccountLoginResponsePacket response
				= ::server::net::BuildAccountLoginResponse(
					std::move(loginResult)
				);

			ExpectFailureResponse(
				result,
				response,
				common::packet::AccountLoginResponseStatus::ServerError,
				"AccountPacketMapper: database failure"
			);
		}

		return result;
	}
}