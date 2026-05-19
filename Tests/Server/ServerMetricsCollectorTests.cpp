#include "ServerMetricsCollectorTests.h"

#include <Server/Diagnostics/ServerMetricsCollector.h>

namespace
{
	void RunInitialSnapshotIsZeroTest(common::diagnostics::DebugTestResult& result)
	{
		server::diagnostics::ServerMetricsCollector metricsCollector;

		const server::diagnostics::ServerMetricsSnapshot snapshot = metricsCollector.CaptureSnapshot();

		common::diagnostics::Expect(result, snapshot.receivedPacketCount == 0, "ServerMetricsCollector: initial received packet count");
		common::diagnostics::Expect(result, snapshot.invalidPacketDropCount == 0, "ServerMetricsCollector: initial invalid packet count");
		common::diagnostics::Expect(result, snapshot.joinRequestCount == 0, "ServerMetricsCollector: initial join request count");
		common::diagnostics::Expect(result, snapshot.inputCommandCount == 0, "ServerMetricsCollector: initial input command count");
		common::diagnostics::Expect(result, snapshot.fireRequestCount == 0, "ServerMetricsCollector: initial fire request count");
		common::diagnostics::Expect(result, snapshot.leaveRequestCount == 0, "ServerMetricsCollector: initial leave request count");
		common::diagnostics::Expect(result, snapshot.joinRoomRequestCount == 0, "ServerMetricsCollector: initial join room request count");
		common::diagnostics::Expect(result, snapshot.playerSnapshotSendRequestCount == 0, "ServerMetricsCollector: player snapshot send request count");
		common::diagnostics::Expect(result, snapshot.bulletSnapshotSendRequestCount == 0, "ServerMetricsCollector: bullet snapshot send request count");
		common::diagnostics::Expect(result, snapshot.impactEffectSendRequestCount == 0, "ServerMetricsCollector: impact effect send request count");
		common::diagnostics::Expect(result, snapshot.timedOutPeerCount == 0, "ServerMetricsCollector: initial timed out peer count");
	}

	void RunIncrementCountersTest(common::diagnostics::DebugTestResult& result)
	{
		server::diagnostics::ServerMetricsCollector metricsCollector;

		metricsCollector.IncrementReceivedPacketCount();
		metricsCollector.IncrementReceivedPacketCount();
		metricsCollector.IncrementInvalidPacketDropCount();

		metricsCollector.IncrementJoinRequestCount();
		metricsCollector.IncrementInputCommandCount();
		metricsCollector.IncrementInputCommandCount();
		metricsCollector.IncrementFireRequestCount();
		metricsCollector.IncrementLeaveRequestCount();
		metricsCollector.IncrementJoinRoomRequestCount();

		metricsCollector.AddPlayerSnapshotSendRequestCount(3);
		metricsCollector.AddBulletSnapshotSendRequestCount(4);
		metricsCollector.AddImpactEffectSendRequestCount(5);

		metricsCollector.AddTimedOutPeerCount(2);

		const server::diagnostics::ServerMetricsSnapshot snapshot = metricsCollector.CaptureSnapshot();

		common::diagnostics::Expect(result, snapshot.receivedPacketCount == 2, "ServerMetricsCollector: received packet count");
		common::diagnostics::Expect(result, snapshot.invalidPacketDropCount == 1, "ServerMetricsCollector: invalid packet count");
		common::diagnostics::Expect(result, snapshot.joinRequestCount == 1, "ServerMetricsCollector: join request count");
		common::diagnostics::Expect(result, snapshot.inputCommandCount == 2, "ServerMetricsCollector: input command count");
		common::diagnostics::Expect(result, snapshot.fireRequestCount == 1, "ServerMetricsCollector: fire request count");
		common::diagnostics::Expect(result, snapshot.leaveRequestCount == 1, "ServerMetricsCollector: leave request count");
		common::diagnostics::Expect(result, snapshot.joinRoomRequestCount == 1, "ServerMetricsCollector: join room request count");
		common::diagnostics::Expect(result, snapshot.playerSnapshotSendRequestCount == 3, "ServerMetricsCollector: player snapshot send request count");
		common::diagnostics::Expect(result, snapshot.bulletSnapshotSendRequestCount == 4, "ServerMetricsCollector: bullet snapshot send request count");
		common::diagnostics::Expect(result, snapshot.impactEffectSendRequestCount == 5, "ServerMetricsCollector: impact effect send request count");
		common::diagnostics::Expect(result, snapshot.timedOutPeerCount == 2, "ServerMetricsCollector: timed out peer count");
	}

	void RunResetTest(common::diagnostics::DebugTestResult& result)
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

		metricsCollector.Reset();

		const server::diagnostics::ServerMetricsSnapshot snapshot = metricsCollector.CaptureSnapshot();

		common::diagnostics::Expect(result, snapshot.receivedPacketCount == 0, "ServerMetricsCollector: reset received packet count");
		common::diagnostics::Expect(result, snapshot.invalidPacketDropCount == 0, "ServerMetricsCollector: reset invalid packet count");
		common::diagnostics::Expect(result, snapshot.joinRequestCount == 0, "ServerMetricsCollector: reset join request count");
		common::diagnostics::Expect(result, snapshot.inputCommandCount == 0, "ServerMetricsCollector: reset input command count");
		common::diagnostics::Expect(result, snapshot.fireRequestCount == 0, "ServerMetricsCollector: reset fire request count");
		common::diagnostics::Expect(result, snapshot.leaveRequestCount == 0, "ServerMetricsCollector: reset leave request count");
		common::diagnostics::Expect(result, snapshot.joinRoomRequestCount == 0, "ServerMetricsCollector: reset join room request count");
		common::diagnostics::Expect(result, snapshot.playerSnapshotSendRequestCount == 0, "ServerMetricsCollector: reset player snapshot send request count");
		common::diagnostics::Expect(result, snapshot.bulletSnapshotSendRequestCount == 0, "ServerMetricsCollector: reset bullet snapshot send request count");
		common::diagnostics::Expect(result, snapshot.impactEffectSendRequestCount == 0, "ServerMetricsCollector: reset impact effect send request count");
		common::diagnostics::Expect(result, snapshot.timedOutPeerCount == 0, "ServerMetricsCollector: reset timed out peer count");
	}
}

namespace tests::server
{
	common::diagnostics::DebugTestResult RunServerMetricsCollectorTests()
	{
		common::diagnostics::DebugTestResult result{};

		RunInitialSnapshotIsZeroTest(result);
		RunIncrementCountersTest(result);
		RunResetTest(result);

		return result;
	}
}