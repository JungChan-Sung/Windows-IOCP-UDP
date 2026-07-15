#include "TestRunner.h"

#include <iostream>
#include <string>

#include "Client/ClientConfigTests.h"
#include "Client/ClientWorldTests.h"
#include "Client/JoinHandshakeStateTests.h"
#include "Client/SnapshotChunkAssemblerTests.h"
#include "Config/ConfigTextTests.h"
#include "Net/SnapshotChunkAssemblerCoreTests.h"
#include "Net/ReliableUdpProtocolTests.h"
#include "Net/ReliableUdpPacketHeaderTests.h"
#include "Net/ReliableUdpPacketBuilderTests.h"
#include "Net/ReliableUdpSendWindowTests.h"
#include "Net/ReliableUdpSessionTests.h"
#include "Net/ReliableUdpLoadTests.h"
#include "Net/UdpFaultDecisionGeneratorTests.h"
#include "Net/UdpFaultPacketSchedulerTests.h"
#include "Net/UdpFaultSimulatorTests.h"
#include "Packet/PacketSerializationTests.h"
#include "Packet/PacketReliabilityTests.h"
#include "Persistence/AccountRepositoryIntegrationTests.h"
#include "Persistence/PersistenceRuntimeTests.h"
#include "Threading/ThreadPoolTests.h"
#include "Log/AsyncLogWriterTests.h"
#include "Log/LogFormatterTests.h"
#include "Log/LogLevelTests.h"
#include "Log/LogMessageBuilderTests.h"
#include "Server/ServerConfigTests.h"
#include "Server/ServerMetricsCollectorTests.h"
#include "Server/GameSimulationTests.h"
#include "Server/PeerSessionServiceTests.h"
#include "Server/PlayerCommandServiceTests.h"
#include "Server/PacketPayloadValidatorTests.h"
#include "Server/UdpPacketDispatcherTests.h"
#include "Server/SnapshotBroadcastBuilderTests.h"
#include "Server/IntegrationSmokeTests.h"
#include "Server/InvalidPacketLogLimiterTests.h"
#include "Game/WorldCollisionTests.h"

namespace tests
{
	bool TestRunner::RunAll()
	{
		tests::DebugTestResult totalResult{};

		MergeAndPrint(totalResult, "PacketSerialization", packet::RunPacketSerializationTests());
		MergeAndPrint(totalResult, "PacketReliability", packet::RunPacketReliabilityTests());
		MergeAndPrint(totalResult, "SnapshotChunkAssemblerCore", net::RunSnapshotChunkAssemblerCoreTests());
		MergeAndPrint(totalResult, "ReliableUdpProtocol", net::RunReliableUdpProtocolTests());
		MergeAndPrint(totalResult, "ReliableUdpPacketHeader", net::RunReliableUdpPacketHeaderTests());
		MergeAndPrint(totalResult, "ReliableUdpPacketBuilder", net::RunReliableUdpPacketBuilderTests());
		MergeAndPrint(totalResult, "ReliableUdpSendWindow", net::RunReliableUdpSendWindowTests());
		MergeAndPrint(totalResult, "ReliableUdpSession", net::RunReliableUdpSessionTests());
		MergeAndPrint(totalResult, "ReliableUdpLoad", net::RunReliableUdpLoadTests());
		MergeAndPrint(totalResult, "UdpFaultDecisionGenerator", net::RunUdpFaultDecisionGeneratorTests());
		MergeAndPrint(totalResult, "UdpFaultPacketScheduler", net::RunUdpFaultPacketSchedulerTests());
		MergeAndPrint(totalResult, "UdpFaultSimulator", net::RunUdpFaultSimulatorTests());
		MergeAndPrint(totalResult, "WorldCollision", game::RunWorldCollisionTests());
		MergeAndPrint(totalResult, "ThreadPool", threading::RunThreadPoolTests());
		MergeAndPrint(totalResult, "LogLevel", log::RunLogLevelTests());
		MergeAndPrint(totalResult, "LogFormatter", log::RunLogFormatterTests());
		MergeAndPrint(totalResult, "LogMessageBuilder", log::RunLogMessageBuilderTests());
		MergeAndPrint(totalResult, "AsyncLogWriter", log::RunAsyncLogWriterTests());
		MergeAndPrint(totalResult, "ConfigText", config::RunConfigTextTests());
		MergeAndPrint(totalResult, "ServerConfig", server::RunServerConfigTests());
		MergeAndPrint(totalResult, "ServerMetricsCollector", server::RunServerMetricsCollectorTests());
		MergeAndPrint(totalResult, "InvalidPacketLogLimiter", server::RunInvalidPacketLogLimiterTests());
		MergeAndPrint(totalResult, "GameSimulation", server::RunGameSimulationTests());
		MergeAndPrint(totalResult, "PeerSessionService", server::RunPeerSessionServiceTests());
		MergeAndPrint(totalResult, "PlayerCommandService", server::RunPlayerCommandServiceTests());
		MergeAndPrint(totalResult, "SnapshotBroadcastBuilder", server::RunSnapshotBroadcastBuilderTests());
		MergeAndPrint(totalResult, "PacketPayloadValidator", server::RunPacketPayloadValidatorTests());
		MergeAndPrint(totalResult, "UdpPacketDispatcher", server::RunUdpPacketDispatcherTests());
		MergeAndPrint(totalResult, "IntegrationSmoke", server::RunIntegrationSmokeTests());
		MergeAndPrint(totalResult, "ClientConfig", client::RunClientConfigTests());
		MergeAndPrint(totalResult, "ClientWorld", client::RunClientWorldTests());
		MergeAndPrint(totalResult, "JoinHandshakeState", client::RunJoinHandshakeStateTests());
		MergeAndPrint(totalResult, "ClientSnapshotChunkAssembler", client::RunSnapshotChunkAssemblerTests());
		MergeAndPrint(totalResult, "PersistenceRuntime", persistence::RunPersistenceRuntimeTests());
		MergeAndPrint(totalResult, "AccountRepositoryIntegration", persistence::RunAccountRepositoryIntegrationTests());

		std::cout << "[Total] Passed=" << totalResult.passedCount << ", Failed=" << totalResult.failedCount << '\n';

		return totalResult.IsSucceeded();
	}

	void TestRunner::PrintResult(std::string_view testName, const tests::DebugTestResult& result)
	{
		for (const std::string& failure : result.failureList)
		{
			std::cerr << "[" << testName << "] " << failure << '\n';
		}

		std::cout << "[" << testName << "] Passed=" << result.passedCount << ", Failed=" << result.failedCount << '\n';
	}

	void TestRunner::MergeAndPrint(
		tests::DebugTestResult& totalResult,
		std::string_view testName,
		const tests::DebugTestResult& result
	)
	{
		PrintResult(testName, result);
		totalResult.Merge(result);
	}
}