#pragma once

#include <Common/Identity/IdentityTypes.h>
#include <Common/Net/Endpoint.h>
#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Net/PeerRoomManager.h>
#include <Server/Service/AuthenticatedAccountRegistry.h>

namespace server::service
{
	class AccountLoginAdmissionService final
	{
	public:
		enum class Status
		{
			Unchanged,
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
		AccountLoginAdmissionService() = default;
		~AccountLoginAdmissionService() noexcept = default;

		AccountLoginAdmissionService(const AccountLoginAdmissionService&) = delete;
		AccountLoginAdmissionService& operator=(const AccountLoginAdmissionService&) = delete;

		AccountLoginAdmissionService(AccountLoginAdmissionService&&) = delete;
		AccountLoginAdmissionService& operator=(AccountLoginAdmissionService&&) = delete;

	private:
		static void SetFailureResponse(
			common::packet::AccountLoginResponsePacket& responsePacket,
			common::packet::AccountLoginResponseStatus status
		) noexcept;

	public:
		[[nodiscard]] Status Apply(
			const EndpointKey& endpointKey,
			common::identity::PersistentPlayerId persistentPlayerId,
			common::packet::AccountLoginResponsePacket& responsePacket,
			TimePoint currentTime,
			AuthenticatedAccountRegistry& authenticatedAccountRegistry,
			const net::PeerRoomManager& peerRoomManager
		) const;
	};
}