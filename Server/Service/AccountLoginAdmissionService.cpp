#include "AccountLoginAdmissionService.h"

#include <optional>
#include <string>

#include <Server/Service/SessionTokenGenerator.h>

namespace server::service
{
	AccountLoginAdmissionService::Result AccountLoginAdmissionService::Apply(const Request& request, AuthenticatedAccountRegistry& authenticatedAccountRegistry, const PeerRoomManager& peerRoomManager) const
	{
		if (request.accountId <= 0 || request.persistentPlayerId <= 0 || request.nickname.empty())
		{
			return Result{
				.status = Status::RegistrationFailed,
			};
		}

		const PeerState* endpointPeerState = peerRoomManager.FindJoinedPeer(request.endpointKey);
		if (endpointPeerState != nullptr)
		{
			if (endpointPeerState->accountId != request.accountId)
			{
				return Result{
					.status = Status::AlreadyLoggedIn,
				};
			}

			if (!common::net::IsValidSessionToken(endpointPeerState->sessionToken))
			{
				return Result{
					.status = Status::RegistrationFailed,
				};
			}

			if (endpointPeerState->persistentPlayerId != request.persistentPlayerId)
			{
				return Result{
					.status = Status::RegistrationFailed,
				};
			}

			static_cast<void>(authenticatedAccountRegistry.Remove(request.endpointKey));

			return Result{
				.status = Status::ExistingSession,
				.sessionToken = endpointPeerState->sessionToken,
			};
		}

		const PeerState* accountPeerState = peerRoomManager.FindJoinedPeerByAccountId(request.accountId);
		if (accountPeerState != nullptr)
		{
			return Result{
				.status = Status::AlreadyLoggedIn,
			};
		}

		const std::optional<EndpointKey> authenticatedEndpointKey = authenticatedAccountRegistry.FindEndpointByAccountId(request.accountId);
		if (authenticatedEndpointKey.has_value() && *authenticatedEndpointKey != request.endpointKey)
		{
			return Result{
				.status = Status::AlreadyLoggedIn,
			};
		}

		const AuthenticatedAccount* endpointAccount = authenticatedAccountRegistry.Find(request.endpointKey);
		if (endpointAccount != nullptr)
		{
			if (endpointAccount->accountId != request.accountId)
			{
				return Result{
					.status = Status::AlreadyLoggedIn,
				};
			}

			if (endpointAccount->persistentPlayerId != request.persistentPlayerId)
			{
				return Result{
					.status = Status::RegistrationFailed,
				};
			}
		}

		const std::optional<common::net::SessionToken> sessionToken = GenerateSessionToken();
		if (!sessionToken.has_value())
		{
			return Result{
				.status = Status::TokenGenerationFailed,
			};
		}

		const bool registered = authenticatedAccountRegistry.Upsert(
			request.endpointKey,
			request.accountId,
			request.persistentPlayerId,
			*sessionToken,
			std::string(request.nickname),
			request.currentTime
		);
		if (!registered)
		{
			return Result{
				.status = Status::RegistrationFailed,
			};
		}

		return Result{
			.status = Status::Authenticated,
			.sessionToken = *sessionToken,
		};
	}
}