#pragma once

#include <WinSock2.h>

#include <chrono>
#include <cstdint>

#include <Common/Net/Endpoint.h>
#include <Common/Net/ReliableUdpSession.h>
#include <Common/Game/GameTypes.h>

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
		std::chrono::steady_clock::time_point lastRecvTime{};
		common::net::ReliableUdpSession reliableSession;
	};
}