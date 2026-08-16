#pragma once

#include <WinSock2.h>

#include <cstdint>
#include <string>

#include <Common/Net/Endpoint.h>
#include <Common/Net/Reliable/ReliableUdpSession.h>
#include <Common/Net/SessionToken.h>
#include <Common/Game/GameTypes.h>
#include <Common/Identity/IdentityTypes.h>
#include <Common/Time/TimeTypes.h>

namespace server::service
{
	struct PeerState
	{
	public:
		sockaddr_in remoteAddress{};
		common::net::EndpointKey endpointKey{};

		common::identity::AccountId accountId = 0;
		common::identity::PersistentPlayerId persistentPlayerId = 0;
		common::net::SessionToken sessionToken{};
		std::string nickname;

		common::game::PlayerId playerId = 0;
		std::uint32_t lastInputSequence = 0;
		common::game::RoomId roomId = 0;
		bool isJoined = false;
		common::time::TimePoint lastRecvTime{};
		common::net::ReliableUdpSession reliableSession;
	};
}