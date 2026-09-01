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

	[[nodiscard]] constexpr common::net::EndpointKey MakeEndpointKey(
		std::uint32_t address,
		std::uint16_t port
	) noexcept
	{
		return common::net::EndpointKey{
			.address = address,
			.port = port,
		};
	}

	void RunRefreshUnknownPeerTest(tests::DebugTestResult& result)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(1, 1000);

		const common::time::TimePoint currentTime =
			common::time::Clock::now();

		const bool refreshed =
			peerRoomManager.RefreshRecvTime(
				endpointKey,
				currentTime
			);

		tests::Expect(
			result,
			!refreshed,
			"PeerRoomManager: unknown peer refresh rejected"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 0,
			"PeerRoomManager: unknown refresh does not create peer"
		);
	}

	void RunRefreshJoinedPeerTest(tests::DebugTestResult& result)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(2, 2000);

		const common::time::TimePoint initialTime =
			common::time::Clock::now();

		const common::time::TimePoint refreshedTime =
			initialTime + common::time::Seconds(2);

		server::service::PeerState& peerState =
			peerRoomManager.UpsertJoinedPeer(
				endpointKey,
				10,
				1,
				initialTime
			);

		tests::Expect(
			result,
			peerState.lastRecvTime == initialTime,
			"PeerRoomManager: joined peer initial receive time"
		);

		const bool refreshed =
			peerRoomManager.RefreshRecvTime(
				endpointKey,
				refreshedTime
			);

		tests::Expect(
			result,
			refreshed,
			"PeerRoomManager: joined peer refresh succeeds"
		);

		const server::service::PeerState* refreshedPeerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			refreshedPeerState != nullptr,
			"PeerRoomManager: refreshed joined peer found"
		);

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

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(3, 3000);

		const common::time::TimePoint initialTime =
			common::time::Clock::now();

		const common::time::TimePoint refreshedTime =
			initialTime + common::time::Seconds(2);

		server::service::PeerState& peerState =
			peerRoomManager.UpsertJoinedPeer(
				endpointKey,
				20,
				1,
				initialTime
			);

		peerState.isJoined = false;

		const bool refreshed =
			peerRoomManager.RefreshRecvTime(
				endpointKey,
				refreshedTime
			);

		tests::Expect(
			result,
			!refreshed,
			"PeerRoomManager: unjoined peer refresh rejected"
		);

		tests::Expect(
			result,
			peerState.lastRecvTime == initialTime,
			"PeerRoomManager: unjoined peer receive time preserved"
		);
	}

	void RunRefreshPreventsTimeoutTest(tests::DebugTestResult& result)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(4, 4000);

		const common::time::TimePoint initialTime =
			common::time::Clock::now();

		const common::time::Seconds timeout{ 10 };

		server::service::PeerState& peerState =
			peerRoomManager.UpsertJoinedPeer(
				endpointKey,
				30,
				2,
				initialTime
			);

		peerState.persistentPlayerId = 300;

		const common::time::TimePoint keepAliveTime =
			initialTime + common::time::Seconds(8);

		const bool refreshed =
			peerRoomManager.RefreshRecvTime(
				endpointKey,
				keepAliveTime
			);

		tests::Expect(
			result,
			refreshed,
			"PeerRoomManager: keep-alive refresh succeeds"
		);

		const PeerRoomManager::RecoverablePeerList retainedPeerList =
			peerRoomManager.MarkTimedOutPeersRecoverable(
				initialTime + common::time::Seconds(15),
				timeout
			);

		tests::Expect(
			result,
			retainedPeerList.empty(),
			"PeerRoomManager: refreshed peer not timed out"
		);

		const server::service::PeerState* retainedPeerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			retainedPeerState != nullptr,
			"PeerRoomManager: refreshed peer retained"
		);

		if (retainedPeerState != nullptr)
		{
			tests::Expect(
				result,
				retainedPeerState->connectionState
				== server::service::PeerConnectionState::Connected,
				"PeerRoomManager: refreshed peer remains connected"
			);
		}

		const PeerRoomManager::RecoverablePeerList recoverablePeerList =
			peerRoomManager.MarkTimedOutPeersRecoverable(
				initialTime + common::time::Seconds(19),
				timeout
			);

		tests::Expect(
			result,
			recoverablePeerList.size() == 1,
			"PeerRoomManager: peer becomes recoverable after refreshed timeout"
		);

		const server::service::PeerState* recoverablePeerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			recoverablePeerState != nullptr,
			"PeerRoomManager: timed out peer remains stored"
		);

		if (recoverablePeerState != nullptr)
		{
			tests::Expect(
				result,
				recoverablePeerState->connectionState
				== server::service::PeerConnectionState::Recoverable,
				"PeerRoomManager: timed out peer is recoverable"
			);
		}

		if (recoverablePeerList.size() == 1)
		{
			const PeerRoomManager::RecoverablePeer& recoverablePeer =
				recoverablePeerList.front();

			tests::Expect(
				result,
				recoverablePeer.endpointKey == endpointKey,
				"PeerRoomManager: recoverable endpoint"
			);

			tests::Expect(
				result,
				recoverablePeer.playerId == 30,
				"PeerRoomManager: recoverable player id"
			);

			tests::Expect(
				result,
				recoverablePeer.persistentPlayerId == 300,
				"PeerRoomManager: recoverable persistent player id"
			);

			tests::Expect(
				result,
				recoverablePeer.roomId == 2,
				"PeerRoomManager: recoverable room id"
			);
		}
	}

	void RunRefreshAtTimeoutBoundaryTest(tests::DebugTestResult& result)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(5, 5000);

		const common::time::TimePoint initialTime =
			common::time::Clock::now();

		const common::time::Seconds timeout{ 10 };

		static_cast<void>(
			peerRoomManager.UpsertJoinedPeer(
				endpointKey,
				40,
				3,
				initialTime
			)
			);

		const PeerRoomManager::RecoverablePeerList boundaryPeerList =
			peerRoomManager.MarkTimedOutPeersRecoverable(
				initialTime + timeout,
				timeout
			);

		tests::Expect(
			result,
			boundaryPeerList.empty(),
			"PeerRoomManager: peer remains connected exactly at timeout boundary"
		);

		const server::service::PeerState* boundaryPeerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			boundaryPeerState != nullptr,
			"PeerRoomManager: boundary peer still joined"
		);

		if (boundaryPeerState != nullptr)
		{
			tests::Expect(
				result,
				boundaryPeerState->connectionState
				== server::service::PeerConnectionState::Connected,
				"PeerRoomManager: boundary peer remains connected"
			);
		}

		const PeerRoomManager::RecoverablePeerList recoverablePeerList =
			peerRoomManager.MarkTimedOutPeersRecoverable(
				initialTime
				+ timeout
				+ common::time::Milliseconds(1),
				timeout
			);

		tests::Expect(
			result,
			recoverablePeerList.size() == 1,
			"PeerRoomManager: peer becomes recoverable after timeout boundary"
		);

		const server::service::PeerState* recoverablePeerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			recoverablePeerState != nullptr,
			"PeerRoomManager: recoverable boundary peer retained"
		);

		if (recoverablePeerState != nullptr)
		{
			tests::Expect(
				result,
				recoverablePeerState->connectionState
				== server::service::PeerConnectionState::Recoverable,
				"PeerRoomManager: peer recoverable after boundary"
			);
		}
	}

	void RunFindJoinedPeerByPlayerIdTest(
		tests::DebugTestResult& result
	)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey1 =
			MakeEndpointKey(10, 1000);

		const common::net::EndpointKey endpointKey2 =
			MakeEndpointKey(20, 2000);

		const common::time::TimePoint currentTime =
			common::time::Clock::now();

		static_cast<void>(
			peerRoomManager.UpsertJoinedPeer(
				endpointKey1,
				100,
				1,
				currentTime
			)
			);

		static_cast<void>(
			peerRoomManager.UpsertJoinedPeer(
				endpointKey2,
				200,
				2,
				currentTime
			)
			);

		const server::service::PeerState* peerState =
			peerRoomManager.FindJoinedPeerByPlayerId(200);

		tests::Expect(
			result,
			peerState != nullptr,
			"PeerRoomManager: joined peer found by player id"
		);

		if (peerState == nullptr)
		{
			return;
		}

		tests::Expect(
			result,
			peerState->endpointKey == endpointKey2,
			"PeerRoomManager: player id lookup returns correct endpoint"
		);

		tests::Expect(
			result,
			peerState->roomId == 2,
			"PeerRoomManager: player id lookup returns correct room"
		);
	}

	void RunFindJoinedPeerByPlayerIdRejectsUnjoinedPeerTest(
		tests::DebugTestResult& result
	)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(30, 3000);

		server::service::PeerState& peerState =
			peerRoomManager.UpsertJoinedPeer(
				endpointKey,
				300,
				1,
				common::time::Clock::now()
			);

		peerState.isJoined = false;

		tests::Expect(
			result,
			peerRoomManager.FindJoinedPeerByPlayerId(300) == nullptr,
			"PeerRoomManager: unjoined peer not found by player id"
		);

		tests::Expect(
			result,
			peerRoomManager.FindJoinedPeerByPlayerId(999) == nullptr,
			"PeerRoomManager: unknown player id not found"
		);
	}

	void RunTimedOutPeerBecomesRecoverableTest(
		tests::DebugTestResult& result
	)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(40, 4000);

		const common::time::TimePoint joinTime =
			common::time::Clock::now();

		server::service::PeerState& peerState =
			peerRoomManager.UpsertJoinedPeer(
				endpointKey,
				400,
				1,
				joinTime
			);

		peerState.persistentPlayerId = 5001;

		const PeerRoomManager::RecoverablePeerList recoverablePeerList =
			peerRoomManager.MarkTimedOutPeersRecoverable(
				joinTime + common::time::Seconds(10),
				common::time::Seconds(5)
			);

		tests::Expect(
			result,
			recoverablePeerList.size() == 1,
			"PeerRoomManager: timed out peer becomes recoverable"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 1,
			"PeerRoomManager: recoverable peer remains stored"
		);

		tests::Expect(
			result,
			peerRoomManager.GetJoinedPeerCount() == 1,
			"PeerRoomManager: recoverable peer remains joined"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(1) == 1,
			"PeerRoomManager: recoverable peer remains room member"
		);

		const server::service::PeerState* recoverablePeer =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			recoverablePeer != nullptr,
			"PeerRoomManager: recoverable peer can be found"
		);

		if (recoverablePeer != nullptr)
		{
			tests::Expect(
				result,
				recoverablePeer->connectionState
				== server::service::PeerConnectionState::Recoverable,
				"PeerRoomManager: recoverable peer connection state"
			);

			tests::Expect(
				result,
				recoverablePeer->recoverableSince
				== joinTime + common::time::Seconds(10),
				"PeerRoomManager: recoverable start time stored"
			);
		}
	}

	void RunRecoverablePeerExcludedFromBroadcastTargetsTest(
		tests::DebugTestResult& result
	)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey connectedEndpointKey =
			MakeEndpointKey(50, 5000);

		const common::net::EndpointKey recoverableEndpointKey =
			MakeEndpointKey(60, 6000);

		const common::time::TimePoint joinTime =
			common::time::Clock::now();

		static_cast<void>(
			peerRoomManager.UpsertJoinedPeer(
				connectedEndpointKey,
				500,
				1,
				joinTime
			)
			);

		static_cast<void>(
			peerRoomManager.UpsertJoinedPeer(
				recoverableEndpointKey,
				600,
				1,
				joinTime
			)
			);

		const bool refreshed =
			peerRoomManager.RefreshRecvTime(
				connectedEndpointKey,
				joinTime + common::time::Seconds(8)
			);

		tests::Expect(
			result,
			refreshed,
			"PeerRoomManager: connected broadcast peer refreshed"
		);

		const PeerRoomManager::RecoverablePeerList recoverablePeerList =
			peerRoomManager.MarkTimedOutPeersRecoverable(
				joinTime + common::time::Seconds(11),
				common::time::Seconds(10)
			);

		tests::Expect(
			result,
			recoverablePeerList.size() == 1,
			"PeerRoomManager: one peer becomes recoverable"
		);

		const PeerRoomManager::EndpointKeyList endpointKeyList =
			peerRoomManager.BuildRoomEndpointKeyList(1);

		tests::Expect(
			result,
			endpointKeyList.size() == 1,
			"PeerRoomManager: recoverable peer excluded from broadcast targets"
		);

		if (endpointKeyList.size() == 1)
		{
			tests::Expect(
				result,
				endpointKeyList.front() == connectedEndpointKey,
				"PeerRoomManager: connected peer remains broadcast target"
			);
		}

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(1) == 2,
			"PeerRoomManager: recoverable peer remains room member"
		);
	}

	void RunRecoverablePeerSurvivesGracePeriodTest(
		tests::DebugTestResult& result
	)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(70, 7000);

		const common::time::TimePoint joinTime =
			common::time::Clock::now();

		const common::time::TimePoint recoverableTime =
			joinTime + common::time::Seconds(11);

		static_cast<void>(
			peerRoomManager.UpsertJoinedPeer(
				endpointKey,
				700,
				1,
				joinTime
			)
			);

		const PeerRoomManager::RecoverablePeerList recoverablePeerList =
			peerRoomManager.MarkTimedOutPeersRecoverable(
				recoverableTime,
				common::time::Seconds(10)
			);

		tests::Expect(
			result,
			recoverablePeerList.size() == 1,
			"PeerRoomManager: grace setup peer becomes recoverable"
		);

		const PeerRoomManager::ExpiredRecoverablePeerList expiredPeerList =
			peerRoomManager.RemoveExpiredRecoverablePeers(
				recoverableTime + common::time::Seconds(30),
				common::time::Seconds(30)
			);

		tests::Expect(
			result,
			expiredPeerList.empty(),
			"PeerRoomManager: recoverable peer survives at grace boundary"
		);

		const server::service::PeerState* peerState =
			peerRoomManager.FindJoinedPeer(endpointKey);

		tests::Expect(
			result,
			peerState != nullptr,
			"PeerRoomManager: recoverable peer retained during grace period"
		);

		if (peerState != nullptr)
		{
			tests::Expect(
				result,
				peerState->connectionState
				== server::service::PeerConnectionState::Recoverable,
				"PeerRoomManager: retained peer remains recoverable"
			);
		}

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(1) == 1,
			"PeerRoomManager: grace peer remains room member"
		);
	}

	void RunExpiredRecoverablePeerIsRemovedTest(
		tests::DebugTestResult& result
	)
	{
		PeerRoomManager peerRoomManager;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(80, 8000);

		const common::time::TimePoint joinTime =
			common::time::Clock::now();

		const common::time::TimePoint recoverableTime =
			joinTime + common::time::Seconds(11);

		server::service::PeerState& peerState =
			peerRoomManager.UpsertJoinedPeer(
				endpointKey,
				800,
				2,
				joinTime
			);

		peerState.persistentPlayerId = 5001;

		const PeerRoomManager::RecoverablePeerList recoverablePeerList =
			peerRoomManager.MarkTimedOutPeersRecoverable(
				recoverableTime,
				common::time::Seconds(10)
			);

		tests::Expect(
			result,
			recoverablePeerList.size() == 1,
			"PeerRoomManager: expired setup peer becomes recoverable"
		);

		const PeerRoomManager::ExpiredRecoverablePeerList expiredPeerList =
			peerRoomManager.RemoveExpiredRecoverablePeers(
				recoverableTime
				+ common::time::Seconds(30)
				+ common::time::Milliseconds(1),
				common::time::Seconds(30)
			);

		tests::Expect(
			result,
			expiredPeerList.size() == 1,
			"PeerRoomManager: recoverable peer expires after grace period"
		);

		tests::Expect(
			result,
			peerRoomManager.FindPeer(endpointKey) == nullptr,
			"PeerRoomManager: expired recoverable peer removed"
		);

		tests::Expect(
			result,
			peerRoomManager.GetPeerCount() == 0,
			"PeerRoomManager: expired peer count"
		);

		tests::Expect(
			result,
			peerRoomManager.GetJoinedPeerCount() == 0,
			"PeerRoomManager: expired joined peer count"
		);

		tests::Expect(
			result,
			peerRoomManager.GetRoomMemberCount(2) == 0,
			"PeerRoomManager: expired peer removed from room"
		);

		if (expiredPeerList.size() == 1)
		{
			const PeerRoomManager::ExpiredRecoverablePeer& expiredPeer =
				expiredPeerList.front();

			tests::Expect(
				result,
				expiredPeer.endpointKey == endpointKey,
				"PeerRoomManager: expired endpoint"
			);

			tests::Expect(
				result,
				expiredPeer.playerId == 800,
				"PeerRoomManager: expired player id"
			);

			tests::Expect(
				result,
				expiredPeer.persistentPlayerId == 5001,
				"PeerRoomManager: expired persistent player id"
			);

			tests::Expect(
				result,
				expiredPeer.roomId == 2,
				"PeerRoomManager: expired room id"
			);
		}
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
		RunFindJoinedPeerByPlayerIdTest(result);
		RunFindJoinedPeerByPlayerIdRejectsUnjoinedPeerTest(result);

		RunTimedOutPeerBecomesRecoverableTest(result);
		RunRecoverablePeerExcludedFromBroadcastTargetsTest(result);
		RunRecoverablePeerSurvivesGracePeriodTest(result);
		RunExpiredRecoverablePeerIsRemovedTest(result);

		return result;
	}
}