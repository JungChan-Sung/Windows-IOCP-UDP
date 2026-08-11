#pragma once

#include <Common/Net/Endpoint.h>
#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Net/AuthenticatedAccountRegistry.h>
#include <Server/Net/PeerRoomManager.h>

namespace server::net
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
			std::int64_t persistentPlayerId,
			common::packet::AccountLoginResponsePacket& responsePacket,
			TimePoint currentTime,
			AuthenticatedAccountRegistry& authenticatedAccountRegistry,
			const PeerRoomManager& peerRoomManager
		) const;
	};
}