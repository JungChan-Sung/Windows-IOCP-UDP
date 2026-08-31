#include "ServerMetricsCollectorTests.h"

#include <Server/Diagnostics/ServerMetricsCollector.h>

namespace
{
	void RunInitialSnapshotIsZeroTest(tests::DebugTestResult& result)
	{
		server::diagnostics::ServerMetricsCollector metricsCollector;

		const server::diagnostics::ServerMetricsSnapshot snapshot = metricsCollector.CaptureSnapshot();

		tests::Expect(result, snapshot.receivedPacketCount == 0, "ServerMetricsCollector: initial received packet count");
		tests::Expect(result, snapshot.invalidPacketDropCount == 0, "ServerMetricsCollector: initial invalid packet count");
		tests::Expect(result, snapshot.joinRequestCount == 0, "ServerMetricsCollector: initial join request count");
		tests::Expect(result, snapshot.inputCommandCount == 0, "ServerMetricsCollector: initial input command count");
		tests::Expect(result, snapshot.fireRequestCount == 0, "ServerMetricsCollector: initial fire request count");
		tests::Expect(result, snapshot.leaveRequestCount == 0, "ServerMetricsCollector: initial leave request count");
		tests::Expect(result, snapshot.joinRoomRequestCount == 0, "ServerMetricsCollector: initial join room request count");
		tests::Expect(result, snapshot.playerSnapshotSendRequestCount == 0, "ServerMetricsCollector: player snapshot send request count");
		tests::Expect(result, snapshot.bulletSnapshotSendRequestCount == 0, "ServerMetricsCollector: bullet snapshot send request count");
		tests::Expect(result, snapshot.impactEffectSendRequestCount == 0, "ServerMetricsCollector: impact effect send request count");
		tests::Expect(result, snapshot.timedOutPeerCount == 0, "ServerMetricsCollector: initial timed out peer count");
		tests::Expect(result, snapshot.reliableResendGiveUpPacketCount == 0, "ServerMetricsCollector: initial reliable resend give-up packet count");
		tests::Expect(result, snapshot.reliableSendWindowFullCount == 0, "ServerMetricsCollector: initial reliable send window full count");
		tests::Expect(result, snapshot.reliableUnknownPeerPacketCount == 0, "ServerMetricsCollector: initial reliable unknown peer packet count");
		tests::Expect(result, snapshot.reliableUnknownPeerAckOnlyPacketCount == 0, "ServerMetricsCollector: initial reliable unknown peer ack-only packet count");
		tests::Expect(result, snapshot.reliableUnknownPeerDataPacketCount == 0, "ServerMetricsCollector: initial reliable unknown peer data packet count");
		tests::Expect(result, snapshot.invalidReliablePacketCount == 0, "ServerMetricsCollector: initial invalid reliable packet count");
		tests::Expect(result, snapshot.reliableInvalidAckPacketCount == 0, "ServerMetricsCollector: initial reliable invalid ack packet count");
		tests::Expect(
			result,
			snapshot.faultSimulationReleasedSendRequestCount == 0,
			"ServerMetricsCollector: initial fault simulation released send request count"
		);
		tests::Expect(
			result,
			snapshot.authenticationSessionNotFoundDropCount == 0,
			"ServerMetricsCollector: initial authentication session not found drop count"
		);

		tests::Expect(
			result,
			snapshot.authenticationInvalidPacketDropCount == 0,
			"ServerMetricsCollector: initial authentication invalid packet drop count"
		);

		tests::Expect(
			result,
			snapshot.authenticationInvalidTagDropCount == 0,
			"ServerMetricsCollector: initial authentication invalid tag drop count"
		);

		tests::Expect(
			result,
			snapshot.authenticationReplayDropCount == 0,
			"ServerMetricsCollector: initial authentication replay drop count"
		);
	}

	void RunIncrementCountersTest(tests::DebugTestResult& result)
	{
		server::diagnostics::ServerMetricsCollector metricsCollector;

		metricsCollector.IncrementReceivedPacketCount();
		metricsCollector.IncrementReceivedPacketCount();
		metricsCollector.IncrementInvalidPacketDropCount();

		metricsCollector.IncrementAuthenticationSessionNotFoundDropCount();
		metricsCollector.IncrementAuthenticationInvalidPacketDropCount();

		metricsCollector.IncrementAuthenticationInvalidTagDropCount();
		metricsCollector.IncrementAuthenticationInvalidTagDropCount();

		metricsCollector.IncrementAuthenticationReplayDropCount();
		metricsCollector.IncrementAuthenticationReplayDropCount();
		metricsCollector.IncrementAuthenticationReplayDropCount();

		metricsCollector.IncrementJoinRequestCount();
		metricsCollector.IncrementInputCommandCount();
		metricsCollector.IncrementInputCommandCount();
		metricsCollector.IncrementFireRequestCount();
		metricsCollector.IncrementLeaveRequestCount();
		metricsCollector.IncrementJoinRoomRequestCount();
		metricsCollector.AddReliableResendGiveUpPacketCount(3);
		metricsCollector.IncrementReliableSendWindowFullCount();
		metricsCollector.IncrementReliableSendWindowFullCount();
		metricsCollector.IncrementReliableUnknownPeerPacketCount();
		metricsCollector.IncrementReliableUnknownPeerAckOnlyPacketCount();
		metricsCollector.IncrementReliableUnknownPeerDataPacketCount();
		metricsCollector.IncrementInvalidReliablePacketCount();
		metricsCollector.IncrementReliableInvalidAckPacketCount();
		metricsCollector.AddFaultSimulationReleasedSendRequestCount(6);

		metricsCollector.AddPlayerSnapshotSendRequestCount(3);
		metricsCollector.AddBulletSnapshotSendRequestCount(4);
		metricsCollector.AddImpactEffectSendRequestCount(5);

		metricsCollector.AddTimedOutPeerCount(2);

		const server::diagnostics::ServerMetricsSnapshot snapshot = metricsCollector.CaptureSnapshot();

		tests::Expect(result, snapshot.receivedPacketCount == 2, "ServerMetricsCollector: received packet count");
		tests::Expect(result, snapshot.invalidPacketDropCount == 1, "ServerMetricsCollector: invalid packet count");
		tests::Expect(result, snapshot.joinRequestCount == 1, "ServerMetricsCollector: join request count");
		tests::Expect(result, snapshot.inputCommandCount == 2, "ServerMetricsCollector: input command count");
		tests::Expect(result, snapshot.fireRequestCount == 1, "ServerMetricsCollector: fire request count");
		tests::Expect(result, snapshot.leaveRequestCount == 1, "ServerMetricsCollector: leave request count");
		tests::Expect(result, snapshot.joinRoomRequestCount == 1, "ServerMetricsCollector: join room request count");
		tests::Expect(result, snapshot.playerSnapshotSendRequestCount == 3, "ServerMetricsCollector: player snapshot send request count");
		tests::Expect(result, snapshot.bulletSnapshotSendRequestCount == 4, "ServerMetricsCollector: bullet snapshot send request count");
		tests::Expect(result, snapshot.impactEffectSendRequestCount == 5, "ServerMetricsCollector: impact effect send request count");
		tests::Expect(result, snapshot.timedOutPeerCount == 2, "ServerMetricsCollector: timed out peer count");
		tests::Expect(result, snapshot.reliableResendGiveUpPacketCount == 3, "ServerMetricsCollector: reliable resend give-up packet count");
		tests::Expect(result, snapshot.reliableSendWindowFullCount == 2, "ServerMetricsCollector: reliable send window full count");
		tests::Expect(result, snapshot.reliableUnknownPeerPacketCount == 1, "ServerMetricsCollector: reliable unknown peer packet count");
		tests::Expect(result, snapshot.reliableUnknownPeerAckOnlyPacketCount == 1, "ServerMetricsCollector: reliable unknown peer ack-only packet count");
		tests::Expect(result, snapshot.reliableUnknownPeerDataPacketCount == 1, "ServerMetricsCollector: reliable unknown peer data packet count");
		tests::Expect(result, snapshot.invalidReliablePacketCount == 1, "ServerMetricsCollector: invalid reliable packet count");
		tests::Expect(result, snapshot.reliableInvalidAckPacketCount == 1, "ServerMetricsCollector: reliable invalid ack packet count");
		tests::Expect(
			result,
			snapshot.faultSimulationReleasedSendRequestCount == 6,
			"ServerMetricsCollector: fault simulation released send request count"
		);
		tests::Expect(
			result,
			snapshot.authenticationSessionNotFoundDropCount == 1,
			"ServerMetricsCollector: authentication session not found drop count"
		);

		tests::Expect(
			result,
			snapshot.authenticationInvalidPacketDropCount == 1,
			"ServerMetricsCollector: authentication invalid packet drop count"
		);

		tests::Expect(
			result,
			snapshot.authenticationInvalidTagDropCount == 2,
			"ServerMetricsCollector: authentication invalid tag drop count"
		);

		tests::Expect(
			result,
			snapshot.authenticationReplayDropCount == 3,
			"ServerMetricsCollector: authentication replay drop count"
		);
	}

	void RunResetTest(tests::DebugTestResult& result)
	{
		server::diagnostics::ServerMetricsCollector metricsCollector;

		metricsCollector.IncrementReceivedPacketCount();
		metricsCollector.IncrementInvalidPacketDropCount();
		metricsCollector.IncrementJoinRequestCount();
		metricsCollector.IncrementInputCommandCount();
		metricsCollector.IncrementFireRequestCount();
		metricsCollector.IncrementLeaveRequestCount();
		metricsCollector.IncrementJoinRoomRequestCount();
		metricsCollector.AddPlayerSnapshotSendRequestCount(10);
		metricsCollector.AddBulletSnapshotSendRequestCount(10);
		metricsCollector.AddImpactEffectSendRequestCount(10);
		metricsCollector.AddTimedOutPeerCount(10);
		metricsCollector.AddReliableResendGiveUpPacketCount(10);
		metricsCollector.IncrementReliableSendWindowFullCount();
		metricsCollector.IncrementReliableUnknownPeerPacketCount();
		metricsCollector.IncrementReliableUnknownPeerAckOnlyPacketCount();
		metricsCollector.IncrementReliableUnknownPeerDataPacketCount();
		metricsCollector.IncrementInvalidReliablePacketCount();
		metricsCollector.IncrementReliableInvalidAckPacketCount();
		metricsCollector.AddFaultSimulationReleasedSendRequestCount(10);
		metricsCollector.IncrementAuthenticationSessionNotFoundDropCount();
		metricsCollector.IncrementAuthenticationInvalidPacketDropCount();
		metricsCollector.IncrementAuthenticationInvalidTagDropCount();
		metricsCollector.IncrementAuthenticationReplayDropCount();

		metricsCollector.Reset();

		const server::diagnostics::ServerMetricsSnapshot snapshot = metricsCollector.CaptureSnapshot();

		tests::Expect(result, snapshot.receivedPacketCount == 0, "ServerMetricsCollector: reset received packet count");
		tests::Expect(result, snapshot.invalidPacketDropCount == 0, "ServerMetricsCollector: reset invalid packet count");
		tests::Expect(result, snapshot.joinRequestCount == 0, "ServerMetricsCollector: reset join request count");
		tests::Expect(result, snapshot.inputCommandCount == 0, "ServerMetricsCollector: reset input command count");
		tests::Expect(result, snapshot.fireRequestCount == 0, "ServerMetricsCollector: reset fire request count");
		tests::Expect(result, snapshot.leaveRequestCount == 0, "ServerMetricsCollector: reset leave request count");
		tests::Expect(result, snapshot.joinRoomRequestCount == 0, "ServerMetricsCollector: reset join room request count");
		tests::Expect(result, snapshot.playerSnapshotSendRequestCount == 0, "ServerMetricsCollector: reset player snapshot send request count");
		tests::Expect(result, snapshot.bulletSnapshotSendRequestCount == 0, "ServerMetricsCollector: reset bullet snapshot send request count");
		tests::Expect(result, snapshot.impactEffectSendRequestCount == 0, "ServerMetricsCollector: reset impact effect send request count");
		tests::Expect(result, snapshot.timedOutPeerCount == 0, "ServerMetricsCollector: reset timed out peer count");
		tests::Expect(result, snapshot.reliableResendGiveUpPacketCount == 0, "ServerMetricsCollector: reset reliable resend give-up packet count");
		tests::Expect(result, snapshot.reliableSendWindowFullCount == 0, "ServerMetricsCollector: reset reliable send window full count");
		tests::Expect(result, snapshot.reliableUnknownPeerPacketCount == 0, "ServerMetricsCollector: reset reliable unknown peer packet count");
		tests::Expect(result, snapshot.reliableUnknownPeerAckOnlyPacketCount == 0, "ServerMetricsCollector: reset reliable unknown peer ack-only packet count");
		tests::Expect(result, snapshot.reliableUnknownPeerDataPacketCount == 0, "ServerMetricsCollector: reset reliable unknown peer data packet count");
		tests::Expect(result, snapshot.invalidReliablePacketCount == 0, "ServerMetricsCollector: reset invalid reliable packet count");
		tests::Expect(result, snapshot.reliableInvalidAckPacketCount == 0, "ServerMetricsCollector: reset reliable invalid ack packet count");
		tests::Expect(
			result,
			snapshot.faultSimulationReleasedSendRequestCount == 0,
			"ServerMetricsCollector: reset fault simulation released send request count"
		);
		tests::Expect(
			result,
			snapshot.authenticationSessionNotFoundDropCount == 0,
			"ServerMetricsCollector: reset authentication session not found drop count"
		);

		tests::Expect(
			result,
			snapshot.authenticationInvalidPacketDropCount == 0,
			"ServerMetricsCollector: reset authentication invalid packet drop count"
		);

		tests::Expect(
			result,
			snapshot.authenticationInvalidTagDropCount == 0,
			"ServerMetricsCollector: reset authentication invalid tag drop count"
		);

		tests::Expect(
			result,
			snapshot.authenticationReplayDropCount == 0,
			"ServerMetricsCollector: reset authentication replay drop count"
		);
	}
}

namespace tests::server
{
	tests::DebugTestResult RunServerMetricsCollectorTests()
	{
		tests::DebugTestResult result{};

		RunInitialSnapshotIsZeroTest(result);
		RunIncrementCountersTest(result);
		RunResetTest(result);

		return result;
	}
}