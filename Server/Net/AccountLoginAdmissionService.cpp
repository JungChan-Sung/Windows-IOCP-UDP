#include "AccountLoginAdmissionService.h"

namespace server::net
{

	void AccountLoginAdmissionService::SetFailureResponse(common::packet::AccountLoginResponsePacket& responsePacket, common::packet::AccountLoginResponseStatus status) noexcept
	{
		responsePacket.status = status;
		responsePacket.accountId = 0;
		responsePacket.sessionToken = common::net::invalidSessionToken;
		responsePacket.nickname.clear();
	}

	AccountLoginAdmissionService::Status AccountLoginAdmissionService::Apply(const EndpointKey& endpointKey, common::packet::AccountLoginResponsePacket& responsePacket, TimePoint currentTime, AuthenticatedAccountRegistry& authenticatedAccountRegistry, const PeerRoomManager& peerRoomManager) const
	{
		using ResponseStatus = common::packet::AccountLoginResponseStatus;

		if (responsePacket.status != ResponseStatus::Succeeded)
		{
			return Status::Unchanged;
		}

		if (responsePacket.accountId <= 0 || responsePacket.nickname.empty())
		{
			SetFailureResponse(responsePacket, ResponseStatus::ServerError);
			return Status::RegistrationFailed;
		}

		const PeerState* endpointPeerState = peerRoomManager.FindJoinedPeer(endpointKey);
		if (endpointPeerState != nullptr)
		{
			if (endpointPeerState->accountId != responsePacket.accountId)
			{
				SetFailureResponse(responsePacket, ResponseStatus::AlreadyLoggedIn);
				return Status::AlreadyLoggedIn;
			}

			static_cast<void>(authenticatedAccountRegistry.Remove(endpointKey));

			return Status::ExistingSession;
		}

		const PeerState* accountPeerState = peerRoomManager.FindJoinedPeerByAccountId(responsePacket.accountId);
		if (accountPeerState != nullptr)
		{
			SetFailureResponse(responsePacket, ResponseStatus::AlreadyLoggedIn);
			return Status::AlreadyLoggedIn;
		}

		const std::optional<EndpointKey> authenticatedEndpointKey = authenticatedAccountRegistry.FindEndpointByAccountId(responsePacket.accountId);
		if (authenticatedEndpointKey.has_value() && *authenticatedEndpointKey != endpointKey)
		{
			SetFailureResponse(responsePacket, ResponseStatus::AlreadyLoggedIn);
			return Status::AlreadyLoggedIn;
		}

		const AuthenticatedAccount* endpointAccount = authenticatedAccountRegistry.Find(endpointKey);
		if (endpointAccount != nullptr && endpointAccount->accountId != responsePacket.accountId)
		{
			SetFailureResponse(responsePacket, ResponseStatus::AlreadyLoggedIn);
			return Status::AlreadyLoggedIn;
		}

		const bool registered = authenticatedAccountRegistry.Upsert(
				endpointKey,
				responsePacket.accountId,
				responsePacket.nickname,
				currentTime
			);
		if (!registered)
		{
			SetFailureResponse(responsePacket, ResponseStatus::ServerError);
			return Status::RegistrationFailed;
		}

		return Status::Authenticated;
	}
}