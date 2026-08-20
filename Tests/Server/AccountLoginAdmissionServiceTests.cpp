#include "AccountLoginAdmissionServiceTests.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <Common/Net/EndpointKey.h>
#include <Common/Net/SessionToken.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Service/AccountLoginAdmissionService.h>
#include <Server/Service/AuthenticatedAccountRegistry.h>
#include <Server/Service/PeerRoomManager.h>
#include <Server/Service/PeerState.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using AccountLoginAdmissionService = server::service::AccountLoginAdmissionService;
	using AuthenticatedAccountRegistry = server::service::AuthenticatedAccountRegistry;
	using PeerRoomManager = server::service::PeerRoomManager;
	using AdmissionRequest = AccountLoginAdmissionService::Request;
	using AdmissionResult = AccountLoginAdmissionService::Result;
	using AdmissionStatus = AccountLoginAdmissionService::Status;

	inline constexpr std::int64_t persistentPlayerId = 5001;
	inline constexpr std::int64_t otherPersistentPlayerId = 5002;

	[[nodiscard]] constexpr common::net::SessionToken MakeSessionToken(std::uint64_t value) noexcept
	{
		return common::net::SessionToken{
			.high = value,
			.low = value ^ 0x5A5A5A5A5A5A5A5AULL,
		};
	}

	[[nodiscard]] constexpr common::net::EndpointKey MakeEndpointKey(std::uint32_t index) noexcept
	{
		return common::net::EndpointKey{
			.address = 0x7F000001 + index,
			.port = static_cast<std::uint16_t>(30000 + index),
		};
	}

	[[nodiscard]] AdmissionRequest MakeRequest(
		const common::net::EndpointKey& endpointKey,
		std::int64_t accountId,
		std::int64_t playerPersistentId,
		std::string_view nickname,
		common::time::TimePoint currentTime = common::time::TimePoint{}
	) noexcept
	{
		return AdmissionRequest{
			.endpointKey = endpointKey,
			.accountId = accountId,
			.persistentPlayerId = playerPersistentId,
			.nickname = nickname,
			.currentTime = currentTime,
		};
	}

	void ExpectFailureResult(
		tests::DebugTestResult& result,
		const AdmissionResult& admissionResult,
		AdmissionStatus expectedStatus,
		std::string_view message
	)
	{
		tests::Expect(result, admissionResult.status == expectedStatus, std::string(message) + ": status");
		tests::Expect(
			result,
			admissionResult.sessionToken == common::net::invalidSessionToken,
			std::string(message) + ": invalid session token"
		);
	}

	server::service::PeerState& AddJoinedPeer(
		PeerRoomManager& peerRoomManager,
		const common::net::EndpointKey& endpointKey,
		std::int64_t accountId,
		std::int64_t playerPersistentId,
		std::string nickname
	)
	{
		server::service::PeerState& peerState =
			peerRoomManager.UpsertJoinedPeer(endpointKey, 1, 1, common::time::TimePoint{});

		peerState.accountId = accountId;
		peerState.persistentPlayerId = playerPersistentId;
		peerState.sessionToken = MakeSessionToken(static_cast<std::uint64_t>(accountId));
		peerState.nickname = std::move(nickname);
		return peerState;
	}

	void RunInvalidSucceededResponseTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(MakeEndpointKey(2), 0, persistentPlayerId, "nickname"), registry, peerRoomManager);

		ExpectFailureResult(result, admissionResult, AdmissionStatus::RegistrationFailed, "AccountLoginAdmissionService: invalid success rejected");
		tests::Expect(result, registry.GetCount() == 0, "AccountLoginAdmissionService: invalid success not registered");
	}

	void RunInvalidPersistentPlayerIdTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(MakeEndpointKey(13), 1001, 0, "nickname"), registry, peerRoomManager);

		ExpectFailureResult(
			result,
			admissionResult,
			AdmissionStatus::RegistrationFailed,
			"AccountLoginAdmissionService: invalid persistent player id rejected"
		);
		tests::Expect(result, registry.GetCount() == 0, "AccountLoginAdmissionService: invalid persistent player id not registered");
	}

	void RunNewAccountAuthenticatedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(3);
		const common::time::TimePoint currentTime = common::time::TimePoint{} + common::time::Seconds(10);

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(endpointKey, 1001, persistentPlayerId, "nickname", currentTime), registry, peerRoomManager);

		tests::Expect(result, admissionResult.status == AdmissionStatus::Authenticated, "AccountLoginAdmissionService: new account authenticated");
		tests::Expect(
			result,
			common::net::IsValidSessionToken(admissionResult.sessionToken),
			"AccountLoginAdmissionService: session token issued"
		);

		const server::service::AuthenticatedAccount* account = registry.Find(endpointKey);

		tests::Expect(result, account != nullptr, "AccountLoginAdmissionService: authenticated account registered");

		if (account != nullptr)
		{
			tests::Expect(result, account->accountId == 1001, "AccountLoginAdmissionService: registered account id");
			tests::Expect(result, account->persistentPlayerId == persistentPlayerId, "AccountLoginAdmissionService: registered persistent player id");
			tests::Expect(result, account->sessionToken == admissionResult.sessionToken, "AccountLoginAdmissionService: result token registered");
			tests::Expect(result, account->nickname == "nickname", "AccountLoginAdmissionService: registered nickname");
			tests::Expect(result, account->authenticatedTime == currentTime, "AccountLoginAdmissionService: registered authentication time");
		}
	}

	void RunSamePendingAccountRefreshTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(4);

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1001,
				persistentPlayerId,
				MakeSessionToken(1001),
				"old_nickname",
				common::time::TimePoint{}
			)
			);

		const common::time::TimePoint refreshedTime = common::time::TimePoint{} + common::time::Seconds(5);

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(endpointKey, 1001, persistentPlayerId, "new_nickname", refreshedTime), registry, peerRoomManager);

		tests::Expect(result, admissionResult.status == AdmissionStatus::Authenticated, "AccountLoginAdmissionService: same pending account refreshed");
		tests::Expect(
			result,
			common::net::IsValidSessionToken(admissionResult.sessionToken),
			"AccountLoginAdmissionService: refresh issues session token"
		);
		tests::Expect(result, registry.GetCount() == 1, "AccountLoginAdmissionService: refresh keeps registry count");

		const server::service::AuthenticatedAccount* account = registry.Find(endpointKey);

		tests::Expect(
			result,
			account != nullptr && account->persistentPlayerId == persistentPlayerId,
			"AccountLoginAdmissionService: refresh preserves persistent player id"
		);
		tests::Expect(
			result,
			account != nullptr && account->sessionToken == admissionResult.sessionToken,
			"AccountLoginAdmissionService: refresh stores result token"
		);
		tests::Expect(
			result,
			account != nullptr && account->nickname == "new_nickname",
			"AccountLoginAdmissionService: refresh updates nickname"
		);
		tests::Expect(
			result,
			account != nullptr && account->authenticatedTime == refreshedTime,
			"AccountLoginAdmissionService: refresh updates time"
		);
	}

	void RunPendingAccountOnOtherEndpointRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey firstEndpointKey = MakeEndpointKey(5);
		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(6);
		const common::net::SessionToken firstSessionToken = MakeSessionToken(1001);

		static_cast<void>(
			registry.Upsert(
				firstEndpointKey,
				1001,
				persistentPlayerId,
				firstSessionToken,
				"first",
				common::time::TimePoint{}
			)
			);

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(secondEndpointKey, 1001, persistentPlayerId, "second"), registry, peerRoomManager);

		ExpectFailureResult(
			result,
			admissionResult,
			AdmissionStatus::AlreadyLoggedIn,
			"AccountLoginAdmissionService: pending account duplicate rejected"
		);

		const server::service::AuthenticatedAccount* firstAccount = registry.Find(firstEndpointKey);

		tests::Expect(
			result,
			firstAccount != nullptr && firstAccount->persistentPlayerId == persistentPlayerId,
			"AccountLoginAdmissionService: original pending persistent player id retained"
		);
		tests::Expect(
			result,
			firstAccount != nullptr && firstAccount->sessionToken == firstSessionToken,
			"AccountLoginAdmissionService: original pending token retained"
		);
		tests::Expect(result, !registry.Contains(secondEndpointKey), "AccountLoginAdmissionService: duplicate endpoint not registered");
	}

	void RunDifferentAccountOnSamePendingEndpointRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(7);
		const common::net::SessionToken firstSessionToken = MakeSessionToken(1001);

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1001,
				persistentPlayerId,
				firstSessionToken,
				"first",
				common::time::TimePoint{}
			)
			);

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(endpointKey, 2002, otherPersistentPlayerId, "second"), registry, peerRoomManager);

		ExpectFailureResult(
			result,
			admissionResult,
			AdmissionStatus::AlreadyLoggedIn,
			"AccountLoginAdmissionService: pending endpoint account switch rejected"
		);

		const server::service::AuthenticatedAccount* account = registry.Find(endpointKey);

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

		const common::net::EndpointKey endpointKey = MakeEndpointKey(14);

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1001,
				persistentPlayerId,
				MakeSessionToken(1001),
				"first",
				common::time::TimePoint{}
			)
			);

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(endpointKey, 1001, otherPersistentPlayerId, "first"), registry, peerRoomManager);

		ExpectFailureResult(
			result,
			admissionResult,
			AdmissionStatus::RegistrationFailed,
			"AccountLoginAdmissionService: mismatched pending persistent player id rejected"
		);
	}

	void RunExistingSessionTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(8);
		const common::net::SessionToken expectedSessionToken = MakeSessionToken(1001);

		AddJoinedPeer(peerRoomManager, endpointKey, 1001, persistentPlayerId, "joined");

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1001,
				persistentPlayerId,
				MakeSessionToken(2001),
				"pending",
				common::time::TimePoint{}
			)
			);

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(endpointKey, 1001, persistentPlayerId, "joined"), registry, peerRoomManager);

		tests::Expect(result, admissionResult.status == AdmissionStatus::ExistingSession, "AccountLoginAdmissionService: existing session accepted");
		tests::Expect(
			result,
			admissionResult.sessionToken == expectedSessionToken,
			"AccountLoginAdmissionService: existing session token returned"
		);
		tests::Expect(result, !registry.Contains(endpointKey), "AccountLoginAdmissionService: existing session clears temporary authentication");
	}

	void RunExistingSessionWithMismatchedPersistentPlayerIdTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(15);

		AddJoinedPeer(peerRoomManager, endpointKey, 1001, persistentPlayerId, "joined");

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(endpointKey, 1001, otherPersistentPlayerId, "joined"), registry, peerRoomManager);

		ExpectFailureResult(
			result,
			admissionResult,
			AdmissionStatus::RegistrationFailed,
			"AccountLoginAdmissionService: existing session persistent player mismatch rejected"
		);
	}

	void RunExistingSessionWithInvalidTokenRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(9);

		server::service::PeerState& peerState =
			AddJoinedPeer(peerRoomManager, endpointKey, 1001, persistentPlayerId, "joined");

		peerState.sessionToken = common::net::invalidSessionToken;

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(endpointKey, 1001, persistentPlayerId, "joined"), registry, peerRoomManager);

		ExpectFailureResult(
			result,
			admissionResult,
			AdmissionStatus::RegistrationFailed,
			"AccountLoginAdmissionService: invalid existing session token rejected"
		);
	}

	void RunDifferentAccountOnJoinedEndpointRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(10);

		AddJoinedPeer(peerRoomManager, endpointKey, 1001, persistentPlayerId, "first");

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(endpointKey, 2002, otherPersistentPlayerId, "second"), registry, peerRoomManager);

		ExpectFailureResult(
			result,
			admissionResult,
			AdmissionStatus::AlreadyLoggedIn,
			"AccountLoginAdmissionService: joined endpoint account switch rejected"
		);
	}

	void RunJoinedAccountOnOtherEndpointRejectedTest(tests::DebugTestResult& result)
	{
		AccountLoginAdmissionService service;
		AuthenticatedAccountRegistry registry;
		PeerRoomManager peerRoomManager;

		AddJoinedPeer(peerRoomManager, MakeEndpointKey(11), 1001, persistentPlayerId, "first");

		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(12);

		const AdmissionResult admissionResult =
			service.Apply(MakeRequest(secondEndpointKey, 1001, persistentPlayerId, "second"), registry, peerRoomManager);

		ExpectFailureResult(
			result,
			admissionResult,
			AdmissionStatus::AlreadyLoggedIn,
			"AccountLoginAdmissionService: joined account duplicate rejected"
		);
		tests::Expect(result, !registry.Contains(secondEndpointKey), "AccountLoginAdmissionService: joined duplicate not registered");
	}
}

namespace tests::server
{
	DebugTestResult RunAccountLoginAdmissionServiceTests()
	{
		DebugTestResult result{};

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