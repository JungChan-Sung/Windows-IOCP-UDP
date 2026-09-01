#pragma once

#include <cstdint>
#include <string>

#include <Common/Game/GameTypes.h>
#include <Common/Identity/IdentityTypes.h>
#include <Common/Net/EndpointKey.h>
#include <Common/Net/SessionToken.h>
#include <Common/Time/TimeTypes.h>

namespace server::service
{
	enum class PeerConnectionState : std::uint8_t
	{
		Connected = 0,
		Recoverable,
	};

	struct PeerState
	{
	public:
		common::net::EndpointKey endpointKey{};

		common::identity::AccountId accountId = 0;
		common::identity::PersistentPlayerId persistentPlayerId = 0;
		common::net::SessionToken sessionToken{};
		std::string nickname;

		common::game::PlayerId playerId = 0;
		std::uint32_t lastAcceptedInputSequence = 0;
		std::uint32_t lastProcessedInputSequence = 0;
		common::game::RoomId roomId = 0;

		bool isJoined = false;
		PeerConnectionState connectionState = PeerConnectionState::Connected;

		common::time::TimePoint lastRecvTime{};
		common::time::TimePoint recoverableSince{};
	};
}