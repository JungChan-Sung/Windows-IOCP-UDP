#include "AccountLoginAdmissionServiceTests.h"

#include <WinSock2.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <Common/Net/Endpoint.h>
#include <Common/Net/SessionToken.h>
#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Net/AccountLoginAdmissionService.h>
#include <Server/Net/AuthenticatedAccountRegistry.h>
#include <Server/Net/PeerRoomManager.h>
#include <Server/Net/PeerState.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using AccountLoginAdmissionService = server::net::AccountLoginAdmissionService;
	using AuthenticatedAccountRegistry = server::net::AuthenticatedAccountRegistry;
	using PeerRoomManager = server::net::PeerRoomManager;
	using ResponsePacket = common::packet::AccountLoginResponsePacket;
	using ResponseStatus = common::packet::AccountLoginResponseStatus;

	inline constexpr std::int64_t persistentPlayerId = 5001;
	inline constexpr std::int64_t otherPersistentPlayerId = 5002;

	[[nodiscard]] constexpr common::net::SessionToken MakeSessionToken(std::uint64_t value) noexcept
	{
		return common::net::SessionToken{
			.high = value,
			.low = value ^ 0x5A5A5A5A5A5A5A5AULL,
		};
	}

	[[nodiscard]] sockaddr_in MakeRemoteAddress(std::uint32_t index) noexcept
	{
		sockaddr_in remoteAddress{};
		remoteAddress.sin_family = AF_INET;
		remoteAddress.sin_addr.S_un.S_addr = ::htonl(0x7F000001 + index);
		remoteAddress.sin_port = ::htons(static_cast<u_short>(30000 + index));
		return remoteAddress;
	}

	[[nodiscard]] common::net::EndpointKey MakeEndpointKey(const sockaddr_in& remoteAddress) noexcept
	{
		return common::net::MakeEndpointKey(remoteAddress);
	}

	[[nodiscard]] ResponsePacket MakeSucceededResponse(common::packet::AccountLoginRequestId requestId, std::int64_t accountId, std::string nickname)
	{
		ResponsePacket responsePacket{};
		responsePacket.requestId = requestId;
		responsePacket.status = ResponseStatus::Succeeded;
		responsePacket.accountId = accountId;
		responsePacket.nickname = std::move(nickname);
		return responsePacket;
	}

	void SetTestSessionToken(ResponsePacket& responsePacket) noexcept
	{
		responsePacket.sessionToken = MakeSessionToken(9999);
	}

	void ExpectFailureDataCleared(tests::DebugTestResult& result, const ResponsePacket& responsePacket, std::string_view message)
	{
		tests::Expect(
			result,
			responsePacket.accountId == 0
			&& responsePacket.sessionToken == common::net::invalidSessionToken
			&& responsePacket.nickname.empty(),
			std::string(message)
		);
	}

	server::net::PeerState& AddJoinedPeer(
		PeerRoomManager& peerRoomManager,
		const sockaddr_in& remoteAddress,
		std::int64_t accountId,
		std::int64_t playerPersistentId,
		std::string nickname
	)
	{
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);

		server::net::PeerState& peerState
			= peerRoomManager.UpsertJoinedPeer(remoteAddress, endpointKey, 1, 1, common::time::TimePoint{});

		peerState.accountId = accountId;
		peerState.persistentPlayerId = playerPersistentId;
		peerState.sessionToken = MakeSessionToken(static_cast<std::uint64_t>(accountId));
		peerState.nickname = std::move(nickname);

		return peerState;
	}

	void RunFailedResponseUnchangedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const sockaddr_in remoteAddress = MakeRemoteAddress(1);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);
		const common::net::SessionToken existingSessionToken = MakeSessionToken(1001);

		static_cast<void>(
			registry.Upsert(endpointKey, 1001, persistentPlayerId, existingSessionToken, "existing", common::time::TimePoint{})
			);

		ResponsePacket responsePacket{};
		responsePacket.requestId = 1;
		responsePacket.status = ResponseStatus::InvalidCredentials;

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, persistentPlayerId, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::Unchanged,
			"AccountLoginAdmissionService: failed response unchanged");
		tests::Expect(result, responsePacket.status == ResponseStatus::InvalidCredentials,
			"AccountLoginAdmissionService: failed status preserved");
		tests::Expect(result, responsePacket.sessionToken == common::net::invalidSessionToken,
			"AccountLoginAdmissionService: failed response has invalid token");

		const server::net::AuthenticatedAccount* account = registry.Find(endpointKey);

		tests::Expect(result, account != nullptr, "AccountLoginAdmissionService: failed response preserves authentication");
		tests::Expect(result, account != nullptr && account->persistentPlayerId == persistentPlayerId,
			"AccountLoginAdmissionService: failed response preserves persistent player id");
		tests::Expect(result, account != nullptr && account->sessionToken == existingSessionToken,
			"AccountLoginAdmissionService: failed response preserves existing token");
	}

	void RunInvalidSucceededResponseTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(MakeRemoteAddress(2));

		ResponsePacket responsePacket = MakeSucceededResponse(2, 0, "nickname");
		SetTestSessionToken(responsePacket);

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, persistentPlayerId, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::RegistrationFailed,
			"AccountLoginAdmissionService: invalid success rejected");
		tests::Expect(result, responsePacket.status == ResponseStatus::ServerError,
			"AccountLoginAdmissionService: invalid success becomes server error");

		ExpectFailureDataCleared(result, responsePacket, "AccountLoginAdmissionService: invalid success data cleared");
		tests::Expect(result, registry.GetCount() == 0, "AccountLoginAdmissionService: invalid success not registered");
	}

	void RunInvalidPersistentPlayerIdTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(MakeRemoteAddress(13));

		ResponsePacket responsePacket = MakeSucceededResponse(11, 1001, "nickname");
		SetTestSessionToken(responsePacket);

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, 0, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::RegistrationFailed,
			"AccountLoginAdmissionService: invalid persistent player id rejected");
		tests::Expect(result, responsePacket.status == ResponseStatus::ServerError,
			"AccountLoginAdmissionService: invalid persistent player id becomes server error");

		ExpectFailureDataCleared(result, responsePacket,
			"AccountLoginAdmissionService: invalid persistent player id clears account data");
		tests::Expect(result, registry.GetCount() == 0,
			"AccountLoginAdmissionService: invalid persistent player id not registered");
	}

	void RunNewAccountAuthenticatedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(MakeRemoteAddress(3));
		const common::time::TimePoint currentTime = common::time::TimePoint{} + common::time::Seconds(10);

		ResponsePacket responsePacket = MakeSucceededResponse(3, 1001, "nickname");

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, persistentPlayerId, responsePacket, currentTime, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::Authenticated,
			"AccountLoginAdmissionService: new account authenticated");
		tests::Expect(result, responsePacket.status == ResponseStatus::Succeeded,
			"AccountLoginAdmissionService: successful response preserved");
		tests::Expect(result, common::net::IsValidSessionToken(responsePacket.sessionToken),
			"AccountLoginAdmissionService: session token issued");

		const server::net::AuthenticatedAccount* account = registry.Find(endpointKey);

		tests::Expect(result, account != nullptr, "AccountLoginAdmissionService: authenticated account registered");

		if (account == nullptr)
		{
			return;
		}

		tests::Expect(result, account->accountId == 1001, "AccountLoginAdmissionService: registered account id");
		tests::Expect(result, account->persistentPlayerId == persistentPlayerId,
			"AccountLoginAdmissionService: registered persistent player id");
		tests::Expect(result, account->sessionToken == responsePacket.sessionToken,
			"AccountLoginAdmissionService: response token registered");
		tests::Expect(result, account->nickname == "nickname", "AccountLoginAdmissionService: registered nickname");
		tests::Expect(result, account->authenticatedTime == currentTime,
			"AccountLoginAdmissionService: registered authentication time");
	}

	void RunSamePendingAccountRefreshTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(MakeRemoteAddress(4));

		static_cast<void>(
			registry.Upsert(endpointKey, 1001, persistentPlayerId, MakeSessionToken(1001), "old_nickname", common::time::TimePoint{})
			);

		const common::time::TimePoint refreshedTime = common::time::TimePoint{} + common::time::Seconds(5);
		ResponsePacket responsePacket = MakeSucceededResponse(4, 1001, "new_nickname");

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, persistentPlayerId, responsePacket, refreshedTime, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::Authenticated,
			"AccountLoginAdmissionService: same pending account refreshed");
		tests::Expect(result, common::net::IsValidSessionToken(responsePacket.sessionToken),
			"AccountLoginAdmissionService: refresh issues session token");
		tests::Expect(result, registry.GetCount() == 1, "AccountLoginAdmissionService: refresh keeps registry count");

		const server::net::AuthenticatedAccount* account = registry.Find(endpointKey);

		tests::Expect(result, account != nullptr && account->persistentPlayerId == persistentPlayerId,
			"AccountLoginAdmissionService: refresh preserves persistent player id");
		tests::Expect(result, account != nullptr && account->sessionToken == responsePacket.sessionToken,
			"AccountLoginAdmissionService: refresh stores response token");
		tests::Expect(result, account != nullptr && account->nickname == "new_nickname",
			"AccountLoginAdmissionService: refresh updates nickname");
		tests::Expect(result, account != nullptr && account->authenticatedTime == refreshedTime,
			"AccountLoginAdmissionService: refresh updates time");
	}

	void RunPendingAccountOnOtherEndpointRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey firstEndpointKey = MakeEndpointKey(MakeRemoteAddress(5));
		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(MakeRemoteAddress(6));
		const common::net::SessionToken firstSessionToken = MakeSessionToken(1001);

		static_cast<void>(
			registry.Upsert(firstEndpointKey, 1001, persistentPlayerId, firstSessionToken, "first", common::time::TimePoint{})
			);

		ResponsePacket responsePacket = MakeSucceededResponse(5, 1001, "second");
		SetTestSessionToken(responsePacket);

		const AccountLoginAdmissionService::Status status
			= service.Apply(secondEndpointKey, persistentPlayerId, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::AlreadyLoggedIn,
			"AccountLoginAdmissionService: pending account duplicate rejected");
		tests::Expect(result, responsePacket.status == ResponseStatus::AlreadyLoggedIn,
			"AccountLoginAdmissionService: pending duplicate response status");

		ExpectFailureDataCleared(result, responsePacket, "AccountLoginAdmissionService: pending duplicate data cleared");

		const server::net::AuthenticatedAccount* firstAccount = registry.Find(firstEndpointKey);

		tests::Expect(result, firstAccount != nullptr && firstAccount->persistentPlayerId == persistentPlayerId,
			"AccountLoginAdmissionService: original pending persistent player id retained");
		tests::Expect(result, firstAccount != nullptr && firstAccount->sessionToken == firstSessionToken,
			"AccountLoginAdmissionService: original pending token retained");
		tests::Expect(result, !registry.Contains(secondEndpointKey),
			"AccountLoginAdmissionService: duplicate endpoint not registered");
	}

	void RunDifferentAccountOnSamePendingEndpointRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(MakeRemoteAddress(7));
		const common::net::SessionToken firstSessionToken = MakeSessionToken(1001);

		static_cast<void>(
			registry.Upsert(endpointKey, 1001, persistentPlayerId, firstSessionToken, "first", common::time::TimePoint{})
			);

		ResponsePacket responsePacket = MakeSucceededResponse(6, 2002, "second");
		SetTestSessionToken(responsePacket);

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, otherPersistentPlayerId, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::AlreadyLoggedIn,
			"AccountLoginAdmissionService: pending endpoint account switch rejected");
		tests::Expect(result, responsePacket.status == ResponseStatus::AlreadyLoggedIn,
			"AccountLoginAdmissionService: pending endpoint rejection status");

		ExpectFailureDataCleared(result, responsePacket, "AccountLoginAdmissionService: pending endpoint rejection data cleared");

		const server::net::AuthenticatedAccount* account = registry.Find(endpointKey);

		tests::Expect(
			result,
			account != nullptr
			&& account->accountId == 1001
			&& account->persistentPlayerId == persistentPlayerId
			&& account->sessionToken == firstSessionToken,
			"AccountLoginAdmissionService: original pending identity retained"
		);
	}

	void RunMismatchedPersistentPlayerOnSamePendingEndpointRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(MakeRemoteAddress(14));

		static_cast<void>(
			registry.Upsert(endpointKey, 1001, persistentPlayerId, MakeSessionToken(1001), "first", common::time::TimePoint{})
			);

		ResponsePacket responsePacket = MakeSucceededResponse(12, 1001, "first");

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, otherPersistentPlayerId, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::RegistrationFailed,
			"AccountLoginAdmissionService: mismatched pending persistent player id rejected");
		tests::Expect(result, responsePacket.status == ResponseStatus::ServerError,
			"AccountLoginAdmissionService: mismatched pending persistent player id becomes server error");

		ExpectFailureDataCleared(result, responsePacket,
			"AccountLoginAdmissionService: mismatched pending persistent player id clears account data");
	}

	void RunExistingSessionTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const sockaddr_in remoteAddress = MakeRemoteAddress(8);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);
		const common::net::SessionToken expectedSessionToken = MakeSessionToken(1001);

		AddJoinedPeer(peerRoomManager, remoteAddress, 1001, persistentPlayerId, "joined");

		static_cast<void>(
			registry.Upsert(endpointKey, 1001, persistentPlayerId, MakeSessionToken(2001), "pending", common::time::TimePoint{})
			);

		ResponsePacket responsePacket = MakeSucceededResponse(7, 1001, "joined");

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, persistentPlayerId, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::ExistingSession,
			"AccountLoginAdmissionService: existing session accepted");
		tests::Expect(result, responsePacket.status == ResponseStatus::Succeeded,
			"AccountLoginAdmissionService: existing session response succeeds");
		tests::Expect(result, responsePacket.sessionToken == expectedSessionToken,
			"AccountLoginAdmissionService: existing session token returned");
		tests::Expect(result, !registry.Contains(endpointKey),
			"AccountLoginAdmissionService: existing session clears temporary authentication");
	}

	void RunExistingSessionWithMismatchedPersistentPlayerIdTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const sockaddr_in remoteAddress = MakeRemoteAddress(15);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);

		AddJoinedPeer(peerRoomManager, remoteAddress, 1001, persistentPlayerId, "joined");

		ResponsePacket responsePacket = MakeSucceededResponse(13, 1001, "joined");

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, otherPersistentPlayerId, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::RegistrationFailed,
			"AccountLoginAdmissionService: existing session persistent player mismatch rejected");
		tests::Expect(result, responsePacket.status == ResponseStatus::ServerError,
			"AccountLoginAdmissionService: existing session persistent player mismatch becomes server error");

		ExpectFailureDataCleared(result, responsePacket,
			"AccountLoginAdmissionService: existing session persistent player mismatch clears account data");
	}

	void RunExistingSessionWithInvalidTokenRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const sockaddr_in remoteAddress = MakeRemoteAddress(9);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);

		server::net::PeerState& peerState = AddJoinedPeer(peerRoomManager, remoteAddress, 1001, persistentPlayerId, "joined");
		peerState.sessionToken = common::net::invalidSessionToken;

		ResponsePacket responsePacket = MakeSucceededResponse(8, 1001, "joined");
		SetTestSessionToken(responsePacket);

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, persistentPlayerId, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::RegistrationFailed,
			"AccountLoginAdmissionService: invalid existing session token rejected");
		tests::Expect(result, responsePacket.status == ResponseStatus::ServerError,
			"AccountLoginAdmissionService: invalid existing token becomes server error");

		ExpectFailureDataCleared(result, responsePacket,
			"AccountLoginAdmissionService: invalid existing token data cleared");
	}

	void RunDifferentAccountOnJoinedEndpointRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const sockaddr_in remoteAddress = MakeRemoteAddress(10);
		const common::net::EndpointKey endpointKey = MakeEndpointKey(remoteAddress);

		AddJoinedPeer(peerRoomManager, remoteAddress, 1001, persistentPlayerId, "first");

		ResponsePacket responsePacket = MakeSucceededResponse(9, 2002, "second");
		SetTestSessionToken(responsePacket);

		const AccountLoginAdmissionService::Status status
			= service.Apply(endpointKey, otherPersistentPlayerId, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::AlreadyLoggedIn,
			"AccountLoginAdmissionService: joined endpoint account switch rejected");
		tests::Expect(result, responsePacket.status == ResponseStatus::AlreadyLoggedIn,
			"AccountLoginAdmissionService: joined endpoint rejection status");

		ExpectFailureDataCleared(result, responsePacket,
			"AccountLoginAdmissionService: joined endpoint rejection data cleared");
	}

	void RunJoinedAccountOnOtherEndpointRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const sockaddr_in firstRemoteAddress = MakeRemoteAddress(11);

		AddJoinedPeer(peerRoomManager, firstRemoteAddress, 1001, persistentPlayerId, "first");

		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(MakeRemoteAddress(12));

		ResponsePacket responsePacket = MakeSucceededResponse(10, 1001, "second");
		SetTestSessionToken(responsePacket);

		const AccountLoginAdmissionService::Status status
			= service.Apply(secondEndpointKey, persistentPlayerId, responsePacket, common::time::TimePoint{}, registry, peerRoomManager);

		tests::Expect(result, status == AccountLoginAdmissionService::Status::AlreadyLoggedIn,
			"AccountLoginAdmissionService: joined account duplicate rejected");
		tests::Expect(result, responsePacket.status == ResponseStatus::AlreadyLoggedIn,
			"AccountLoginAdmissionService: joined account duplicate status");

		ExpectFailureDataCleared(result, responsePacket, "AccountLoginAdmissionService: joined duplicate data cleared");
		tests::Expect(result, !registry.Contains(secondEndpointKey),
			"AccountLoginAdmissionService: joined duplicate not registered");
	}
}

namespace tests::server
{
	DebugTestResult RunAccountLoginAdmissionServiceTests()
	{
		DebugTestResult result{};

		RunFailedResponseUnchangedTest(result);
		RunInvalidSucceededResponseTest(result);
		RunInvalidPersistentPlayerIdTest(result);
		RunNewAccountAuthenticatedTest(result);
		RunSamePendingAccountRefreshTest(result);
		RunPendingAccountOnOtherEndpointRejectedTest(result);
		RunDifferentAccountOnSamePendingEndpointRejectedTest(result);
		RunMismatchedPersistentPlayerOnSamePendingEndpointRejectedTest(result);
		RunExistingSessionTest(result);
		RunExistingSessionWithMismatchedPersistentPlayerIdTest(result);
		RunExistingSessionWithInvalidTokenRejectedTest(result);
		RunDifferentAccountOnJoinedEndpointRejectedTest(result);
		RunJoinedAccountOnOtherEndpointRejectedTest(result);

		return result;
	}
}