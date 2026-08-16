#include "AccountLoginAdmissionService.h"

#include <optional>

#include <Server/Service/SessionTokenGenerator.h>

namespace server::service
{

	void AccountLoginAdmissionService::SetFailureResponse(common::packet::AccountLoginResponsePacket& responsePacket, common::packet::AccountLoginResponseStatus status) noexcept
	{
		responsePacket.status = status;
		responsePacket.accountId = 0;
		responsePacket.sessionToken = common::net::invalidSessionToken;
		responsePacket.nickname.clear();
	}

	AccountLoginAdmissionService::Status AccountLoginAdmissionService::Apply(const EndpointKey& endpointKey, common::identity::PersistentPlayerId persistentPlayerId, common::packet::AccountLoginResponsePacket& responsePacket, TimePoint currentTime, AuthenticatedAccountRegistry& authenticatedAccountRegistry, const net::PeerRoomManager& peerRoomManager) const
	{
		using ResponseStatus = common::packet::AccountLoginResponseStatus;

		if (responsePacket.status != ResponseStatus::Succeeded)
		{
			return Status::Unchanged;
		}

		if (responsePacket.accountId <= 0 || persistentPlayerId <= 0 || responsePacket.nickname.empty())
		{
			SetFailureResponse(responsePacket, ResponseStatus::ServerError);
			return Status::RegistrationFailed;
		}

		const net::PeerState* endpointPeerState = peerRoomManager.FindJoinedPeer(endpointKey);
		if (endpointPeerState != nullptr)
		{
			if (endpointPeerState->accountId != responsePacket.accountId)
			{
				SetFailureResponse(responsePacket, ResponseStatus::AlreadyLoggedIn);
				return Status::AlreadyLoggedIn;
			}

			if (!common::net::IsValidSessionToken(endpointPeerState->sessionToken))
			{
				SetFailureResponse(responsePacket, ResponseStatus::ServerError);
				return Status::RegistrationFailed;
			}

			if (endpointPeerState->persistentPlayerId != persistentPlayerId)
			{
				SetFailureResponse(responsePacket, ResponseStatus::ServerError);
				return Status::RegistrationFailed;
			}

			responsePacket.sessionToken = endpointPeerState->sessionToken;

			static_cast<void>(authenticatedAccountRegistry.Remove(endpointKey));

			return Status::ExistingSession;
		}

		const net::PeerState* accountPeerState = peerRoomManager.FindJoinedPeerByAccountId(responsePacket.accountId);
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
		if (endpointAccount != nullptr)
		{
			if (endpointAccount->accountId != responsePacket.accountId)
			{
				SetFailureResponse(responsePacket, ResponseStatus::AlreadyLoggedIn);
				return Status::AlreadyLoggedIn;
			}

			if (endpointAccount->persistentPlayerId != persistentPlayerId)
			{
				SetFailureResponse(responsePacket, ResponseStatus::ServerError);
				return Status::RegistrationFailed;
			}
		}

		const std::optional<common::net::SessionToken> sessionToken = GenerateSessionToken();
		if (!sessionToken.has_value())
		{
			SetFailureResponse(responsePacket, ResponseStatus::ServerError);
			return Status::TokenGenerationFailed;
		}

		const bool registered = authenticatedAccountRegistry.Upsert(
			endpointKey,
			responsePacket.accountId,
			persistentPlayerId,
			*sessionToken,
			responsePacket.nickname,
			currentTime
		);
		if (!registered)
		{
			SetFailureResponse(responsePacket, ResponseStatus::ServerError);
			return Status::RegistrationFailed;
		}

		responsePacket.sessionToken = *sessionToken;

		return Status::Authenticated;
	}
}