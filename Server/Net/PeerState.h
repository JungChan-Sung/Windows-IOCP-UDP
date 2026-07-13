#pragma once

#include <WinSock2.h>

#include <cstdint>

#include <Common/Net/Endpoint.h>
#include <Common/Net/Reliable/ReliableUdpSession.h>
#include <Common/Game/GameTypes.h>
#include <Common/Time/TimeTypes.h>

namespace server::net
{
	struct PeerState
	{
	public:
		sockaddr_in remoteAddress{};
		common::net::EndpointKey endpointKey{};
		common::game::PlayerId playerId = 0;
		std::uint32_t lastInputSequence = 0;
		common::game::RoomId roomId = 0;
		bool isJoined = false;
		common::time::TimePoint lastRecvTime{};
		common::net::ReliableUdpSession reliableSession;
	};
}