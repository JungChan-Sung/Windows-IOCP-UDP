#include "PeerRoomManagerTests.h"

#include <cstdint>

#include <Common/Game/GameTypes.h>
#include <Common/Net/EndpointKey.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Service/PeerRoomManager.h>
#include <Server/Service/PeerState.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using PeerRoomManager = server::service::PeerRoomManager;

	[[nodiscard]] constexpr common::net::EndpointKey MakeEndpointKey(std::uint32_t address, std::uint16_t port) noexcept
	{
		return common::net::EndpointKey{
			.address = address,
			.port = port,
		};
	}

	void RunRefreshUnknownPeerTest(tests::DebugTestResult& result)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(1, 1000);
		const common::time::TimePoint currentTime = common::time::Clock::now();

		const bool refreshed = peerRoomManager.RefreshRecvTime(endpointKey, currentTime);

		tests::Expect(result, !refreshed, "PeerRoomManager: unknown peer refresh rejected");
		tests::Expect(result, peerRoomManager.GetPeerCount() == 0, "PeerRoomManager: unknown refresh does not create peer");
	}

	void RunRefreshJoinedPeerTest(tests::DebugTestResult& result)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(2, 2000);
		const common::time::TimePoint initialTime = common::time::Clock::now();
		const common::time::TimePoint refreshedTime = initialTime + common::time::Seconds(2);

		server::service::PeerState& peerState = peerRoomManager.UpsertJoinedPeer(
			endpointKey,
			10,
			1,
			initialTime
		);

		tests::Expect(result, peerState.lastRecvTime == initialTime, "PeerRoomManager: joined peer initial receive time");

		const bool refreshed = peerRoomManager.RefreshRecvTime(endpointKey, refreshedTime);

		tests::Expect(result, refreshed, "PeerRoomManager: joined peer refresh succeeds");

		const server::service::PeerState* refreshedPeerState = peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(result, refreshedPeerState != nullptr, "PeerRoomManager: refreshed joined peer found");

		if (refreshedPeerState != nullptr)
		{
			tests::Expect(
				result,
				refreshedPeerState->lastRecvTime == refreshedTime,
				"PeerRoomManager: joined peer receive time refreshed"
			);
		}
	}

	void RunRefreshUnjoinedPeerTest(tests::DebugTestResult& result)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(3, 3000);
		const common::time::TimePoint initialTime = common::time::Clock::now();
		const common::time::TimePoint refreshedTime = initialTime + common::time::Seconds(2);

		server::service::PeerState& peerState = peerRoomManager.UpsertJoinedPeer(
			endpointKey,
			20,
			1,
			initialTime
		);

		peerState.isJoined = false;

		const bool refreshed = peerRoomManager.RefreshRecvTime(endpointKey, refreshedTime);

		tests::Expect(result, !refreshed, "PeerRoomManager: unjoined peer refresh rejected");
		tests::Expect(result, peerState.lastRecvTime == initialTime, "PeerRoomManager: unjoined peer receive time preserved");
	}

	void RunRefreshPreventsTimeoutTest(tests::DebugTestResult& result)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(4, 4000);

		const common::time::TimePoint initialTime = common::time::Clock::now();
		const common::time::Seconds timeout{ 10 };

		server::service::PeerState& peerState = peerRoomManager.UpsertJoinedPeer(
			endpointKey,
			30,
			2,
			initialTime
		);

		peerState.persistentPlayerId = 300;

		const common::time::TimePoint keepAliveTime = initialTime + common::time::Seconds(8);

		const bool refreshed = peerRoomManager.RefreshRecvTime(endpointKey, keepAliveTime);

		tests::Expect(result, refreshed, "PeerRoomManager: keep-alive refresh succeeds");

		const PeerRoomManager::TimedOutPeerList retainedPeerList =
			peerRoomManager.RemoveTimedOutPeers(
				initialTime + common::time::Seconds(15),
				timeout
			);

		tests::Expect(result, retainedPeerList.empty(), "PeerRoomManager: refreshed peer not timed out");
		tests::Expect(result, peerRoomManager.FindJoinedPeer(endpointKey) != nullptr, "PeerRoomManager: refreshed peer retained");

		const PeerRoomManager::TimedOutPeerList timedOutPeerList =
			peerRoomManager.RemoveTimedOutPeers(
				initialTime + common::time::Seconds(19),
				timeout
			);

		tests::Expect(result, timedOutPeerList.size() == 1, "PeerRoomManager: peer times out after refreshed timeout");
		tests::Expect(result, peerRoomManager.FindPeer(endpointKey) == nullptr, "PeerRoomManager: timed out peer removed");

		if (timedOutPeerList.size() == 1)
		{
			const PeerRoomManager::TimedOutPeer& timedOutPeer = timedOutPeerList.front();

			tests::Expect(result, timedOutPeer.endpointKey == endpointKey, "PeerRoomManager: timed out endpoint");
			tests::Expect(result, timedOutPeer.playerId == 30, "PeerRoomManager: timed out player id");
			tests::Expect(result, timedOutPeer.persistentPlayerId == 300, "PeerRoomManager: timed out persistent player id");
			tests::Expect(result, timedOutPeer.roomId == 2, "PeerRoomManager: timed out room id");
		}
	}

	void RunRefreshAtTimeoutBoundaryTest(tests::DebugTestResult& result)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(5, 5000);

		const common::time::TimePoint initialTime = common::time::Clock::now();
		const common::time::Seconds timeout{ 10 };

		static_cast<void>(peerRoomManager.UpsertJoinedPeer(
			endpointKey,
			40,
			3,
			initialTime
		));

		const PeerRoomManager::TimedOutPeerList boundaryPeerList =
			peerRoomManager.RemoveTimedOutPeers(
				initialTime + timeout,
				timeout
			);

		tests::Expect(result, boundaryPeerList.empty(), "PeerRoomManager: peer retained exactly at timeout boundary");
		tests::Expect(result, peerRoomManager.FindJoinedPeer(endpointKey) != nullptr, "PeerRoomManager: boundary peer still joined");

		const PeerRoomManager::TimedOutPeerList expiredPeerList =
			peerRoomManager.RemoveTimedOutPeers(
				initialTime + timeout + common::time::Milliseconds(1),
				timeout
			);

		tests::Expect(result, expiredPeerList.size() == 1, "PeerRoomManager: peer removed after timeout boundary");
		tests::Expect(result, peerRoomManager.FindPeer(endpointKey) == nullptr, "PeerRoomManager: expired peer removed");
	}
}

namespace tests::server
{
	DebugTestResult RunPeerRoomManagerTests()
	{
		DebugTestResult result{};

		RunRefreshUnknownPeerTest(result);
		RunRefreshJoinedPeerTest(result);
		RunRefreshUnjoinedPeerTest(result);
		RunRefreshPreventsTimeoutTest(result);
		RunRefreshAtTimeoutBoundaryTest(result);

		return result;
	}
}