#pragma once

#include <string_view>

#include <Common/Identity/IdentityTypes.h>
#include <Common/Net/Endpoint.h>
#include <Common/Net/SessionToken.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Service/AuthenticatedAccountRegistry.h>
#include <Server/Service/PeerRoomManager.h>

namespace server::service
{
	class AccountLoginAdmissionService final
	{
	public:
		enum class Status
		{
			Authenticated,
			ExistingSession,
			AlreadyLoggedIn,
			TokenGenerationFailed,
			RegistrationFailed,
		};

	public:
		using EndpointKey = common::net::EndpointKey;
		using TimePoint = common::time::TimePoint;

	public:
		struct Request
		{
		public:
			EndpointKey endpointKey{};
			common::identity::AccountId accountId = 0;
			common::identity::PersistentPlayerId persistentPlayerId = 0;
			std::string_view nickname;
			TimePoint currentTime{};
		};

		struct Result
		{
		public:
			Status status = Status::RegistrationFailed;
			common::net::SessionToken sessionToken = common::net::invalidSessionToken;
		};

	public:
		AccountLoginAdmissionService() = default;
		~AccountLoginAdmissionService() noexcept = default;

		AccountLoginAdmissionService(const AccountLoginAdmissionService&) = delete;
		AccountLoginAdmissionService& operator=(const AccountLoginAdmissionService&) = delete;

		AccountLoginAdmissionService(AccountLoginAdmissionService&&) = delete;
		AccountLoginAdmissionService& operator=(AccountLoginAdmissionService&&) = delete;

	public:
		[[nodiscard]] Result Apply(
			const Request& request,
			AuthenticatedAccountRegistry& authenticatedAccountRegistry,
			const PeerRoomManager& peerRoomManager
		) const;
	};
}