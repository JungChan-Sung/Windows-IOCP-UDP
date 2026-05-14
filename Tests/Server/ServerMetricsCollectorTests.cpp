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
		common::diagnostics::Expect(result, snapshot.playerSnapshotBroadcastCount == 0, "ServerMetricsCollector: initial player snapshot count");
		common::diagnostics::Expect(result, snapshot.bulletSnapshotBroadcastCount == 0, "ServerMetricsCollector: initial bullet snapshot count");
		common::diagnostics::Expect(result, snapshot.impactEffectBroadcastCount == 0, "ServerMetricsCollector: initial impact effect count");
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

		metricsCollector.AddPlayerSnapshotBroadcastCount(3);
		metricsCollector.AddBulletSnapshotBroadcastCount(4);
		metricsCollector.AddImpactEffectBroadcastCount(5);

		metricsCollector.AddTimedOutPeerCount(2);

		const server::diagnostics::ServerMetricsSnapshot snapshot = metricsCollector.CaptureSnapshot();

		common::diagnostics::Expect(result, snapshot.receivedPacketCount == 2, "ServerMetricsCollector: received packet count");
		common::diagnostics::Expect(result, snapshot.invalidPacketDropCount == 1, "ServerMetricsCollector: invalid packet count");
		common::diagnostics::Expect(result, snapshot.joinRequestCount == 1, "ServerMetricsCollector: join request count");
		common::diagnostics::Expect(result, snapshot.inputCommandCount == 2, "ServerMetricsCollector: input command count");
		common::diagnostics::Expect(result, snapshot.fireRequestCount == 1, "ServerMetricsCollector: fire request count");
		common::diagnostics::Expect(result, snapshot.leaveRequestCount == 1, "ServerMetricsCollector: leave request count");
		common::diagnostics::Expect(result, snapshot.joinRoomRequestCount == 1, "ServerMetricsCollector: join room request count");
		common::diagnostics::Expect(result, snapshot.playerSnapshotBroadcastCount == 3, "ServerMetricsCollector: player snapshot count");
		common::diagnostics::Expect(result, snapshot.bulletSnapshotBroadcastCount == 4, "ServerMetricsCollector: bullet snapshot count");
		common::diagnostics::Expect(result, snapshot.impactEffectBroadcastCount == 5, "ServerMetricsCollector: impact effect count");
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
		metricsCollector.AddPlayerSnapshotBroadcastCount(10);
		metricsCollector.AddBulletSnapshotBroadcastCount(10);
		metricsCollector.AddImpactEffectBroadcastCount(10);
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
		common::diagnostics::Expect(result, snapshot.playerSnapshotBroadcastCount == 0, "ServerMetricsCollector: reset player snapshot count");
		common::diagnostics::Expect(result, snapshot.bulletSnapshotBroadcastCount == 0, "ServerMetricsCollector: reset bullet snapshot count");
		common::diagnostics::Expect(result, snapshot.impactEffectBroadcastCount == 0, "ServerMetricsCollector: reset impact effect count");
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