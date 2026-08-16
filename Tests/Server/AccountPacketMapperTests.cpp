#include "AccountPacketMapperTests.h"

#include <string>
#include <string_view>
#include <utility>

#include <Common/Net/SessionToken.h>
#include <Common/Packet/Account/AccountPacket.h>

#include <Persistence/Account/AccountValidation.h>
#include <Persistence/Core/DatabaseError.h>

#include <Server/Account/AccountService.h>
#include <Server/Protocol/AccountPacketMapper.h>
#include <Server/Service/AccountLoginAdmissionService.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void ExpectFailureResponse(
		tests::DebugTestResult& result,
		const common::packet::AccountLoginResponsePacket& response,
		common::packet::AccountLoginRequestId expectedRequestId,
		common::packet::AccountLoginResponseStatus expectedStatus,
		std::string_view testName
	)
	{
		tests::Expect(
			result,
			response.requestId == expectedRequestId,
			std::string(testName) + ": requestId"
		);

		tests::Expect(
			result,
			response.status == expectedStatus,
			std::string(testName) + ": status"
		);

		tests::Expect(
			result,
			response.accountId == 0
			&& response.sessionToken
			== common::net::invalidSessionToken
			&& response.nickname.empty(),
			std::string(testName) + ": clear account data"
		);
	}

	[[nodiscard]] common::packet::AccountLoginResponsePacket
		MakeAdmissionResponse()
	{
		common::packet::AccountLoginResponsePacket response{};
		response.requestId = 2001;
		response.status =
			common::packet::AccountLoginResponseStatus::Succeeded;
		response.accountId = 1001;
		response.nickname = "nickname";
		return response;
	}

	[[nodiscard]] constexpr common::net::SessionToken
		MakeSessionToken() noexcept
	{
		return common::net::SessionToken{
			.high = 0x1122334455667788ULL,
			.low = 0x8877665544332211ULL,
		};
	}

	void RunAdmissionSuccessTest(
		tests::DebugTestResult& result,
		server::service::AccountLoginAdmissionService::Status status,
		std::string_view testName
	)
	{
		common::packet::AccountLoginResponsePacket response =
			MakeAdmissionResponse();

		const common::net::SessionToken sessionToken =
			MakeSessionToken();

		server::protocol::ApplyAccountLoginAdmissionResult(
			server::service::AccountLoginAdmissionService::Result{
				.status = status,
				.sessionToken = sessionToken,
			},
			response
			);

		tests::Expect(
			result,
			response.status
			== common::packet::AccountLoginResponseStatus
			::Succeeded,
			std::string(testName) + ": status"
		);

		tests::Expect(
			result,
			response.accountId == 1001,
			std::string(testName) + ": account id preserved"
		);

		tests::Expect(
			result,
			response.nickname == "nickname",
			std::string(testName) + ": nickname preserved"
		);

		tests::Expect(
			result,
			response.sessionToken == sessionToken,
			std::string(testName) + ": session token"
		);
	}

	void RunAdmissionFailureTest(
		tests::DebugTestResult& result,
		server::service::AccountLoginAdmissionService::Status status,
		common::packet::AccountLoginResponseStatus expectedStatus,
		std::string_view testName
	)
	{
		common::packet::AccountLoginResponsePacket response =
			MakeAdmissionResponse();

		server::protocol::ApplyAccountLoginAdmissionResult(
			server::service::AccountLoginAdmissionService::Result{
				.status = status,
			},
			response
			);

		ExpectFailureResponse(
			result,
			response,
			2001,
			expectedStatus,
			testName
		);
	}
}

namespace tests::server
{
	DebugTestResult RunAccountPacketMapperTests()
	{
		DebugTestResult result{};

		constexpr common::packet::AccountLoginRequestId
			requestId = 1001;

		{
			::server::account::LoginAccountResult loginResult =
				::server::account::AccountLoginRecord{
					.accountId = 1001,
					.loginName = "account",
					.nickname = "nickname",
			};

			const common::packet::AccountLoginResponsePacket
				response =
				::server::protocol::BuildAccountLoginResponse(
					requestId,
					std::move(loginResult)
				);

			tests::Expect(
				result,
				response.requestId == requestId,
				"AccountPacketMapper: success requestId"
			);

			tests::Expect(
				result,
				response.status
				== common::packet::AccountLoginResponseStatus
				::Succeeded,
				"AccountPacketMapper: success status"
			);

			tests::Expect(
				result,
				response.accountId == 1001,
				"AccountPacketMapper: success account id"
			);

			tests::Expect(
				result,
				response.sessionToken
				== common::net::invalidSessionToken,
				"AccountPacketMapper: success token not issued yet"
			);

			tests::Expect(
				result,
				response.nickname == "nickname",
				"AccountPacketMapper: success nickname"
			);
		}

		{
			::server::account::LoginAccountResult loginResult =
				std::unexpected(
					::server::account::LoginAccountError{
						persistence::account::AccountValidationError{
							.field =
								persistence::account::AccountField
									::LoginName,
							.failure =
								persistence::account
									::AccountValidationFailure::Empty,
						}
					}
				);

			const common::packet::AccountLoginResponsePacket
				response =
				::server::protocol::BuildAccountLoginResponse(
					requestId,
					std::move(loginResult)
				);

			ExpectFailureResponse(
				result,
				response,
				requestId,
				common::packet::AccountLoginResponseStatus
				::InvalidRequest,
				"AccountPacketMapper: validation failure"
			);
		}

		{
			::server::account::LoginAccountResult loginResult =
				std::unexpected(
					::server::account::LoginAccountError{
						::server::account::LoginAccountFailure
							::InvalidCredentials
					}
				);

			const common::packet::AccountLoginResponsePacket
				response =
				::server::protocol::BuildAccountLoginResponse(
					requestId,
					std::move(loginResult)
				);

			ExpectFailureResponse(
				result,
				response,
				requestId,
				common::packet::AccountLoginResponseStatus
				::InvalidCredentials,
				"AccountPacketMapper: invalid credentials"
			);
		}

		{
			::server::account::LoginAccountResult loginResult =
				std::unexpected(
					::server::account::LoginAccountError{
						persistence::core::DatabaseError{
							.failure =
								persistence::core::DatabaseFailure
									::ConnectionOpenFailed,
							.message = "Database unavailable.",
						}
					}
				);

			const common::packet::AccountLoginResponsePacket
				response =
				::server::protocol::BuildAccountLoginResponse(
					requestId,
					std::move(loginResult)
				);

			ExpectFailureResponse(
				result,
				response,
				requestId,
				common::packet::AccountLoginResponseStatus
				::ServerError,
				"AccountPacketMapper: database failure"
			);
		}

		RunAdmissionSuccessTest(
			result,
			::server::service::AccountLoginAdmissionService
			::Status::Authenticated,
			"AccountPacketMapper: admission authenticated"
		);

		RunAdmissionSuccessTest(
			result,
			::server::service::AccountLoginAdmissionService
			::Status::ExistingSession,
			"AccountPacketMapper: admission existing session"
		);

		RunAdmissionFailureTest(
			result,
			::server::service::AccountLoginAdmissionService
			::Status::AlreadyLoggedIn,
			common::packet::AccountLoginResponseStatus
			::AlreadyLoggedIn,
			"AccountPacketMapper: admission already logged in"
		);

		RunAdmissionFailureTest(
			result,
			::server::service::AccountLoginAdmissionService
			::Status::TokenGenerationFailed,
			common::packet::AccountLoginResponseStatus
			::ServerError,
			"AccountPacketMapper: admission token generation failure"
		);

		RunAdmissionFailureTest(
			result,
			::server::service::AccountLoginAdmissionService
			::Status::RegistrationFailed,
			common::packet::AccountLoginResponseStatus
			::ServerError,
			"AccountPacketMapper: admission registration failure"
		);

		return result;
	}
}