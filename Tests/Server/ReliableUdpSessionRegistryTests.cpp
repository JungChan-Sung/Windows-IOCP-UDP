#include "ReliableUdpSessionRegistryTests.h"

#include <Common/Net/Endpoint.h>
#include <Common/Net/Reliable/ReliableUdpConfig.h>
#include <Common/Net/Reliable/ReliableUdpPacketHeader.h>
#include <Common/Net/Reliable/ReliableUdpSession.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Net/ReliableUdpSessionRegistry.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using ReliableUdpSessionRegistry = server::net::ReliableUdpSessionRegistry;

	[[nodiscard]] constexpr common::net::EndpointKey MakeEndpointKey(std::uint32_t address, std::uint16_t port) noexcept
	{
		return common::net::EndpointKey{
			.address = address,
			.port = port,
		};
	}

	void RunInitialStateTest(tests::DebugTestResult& result)
	{
		const ReliableUdpSessionRegistry registry;

		tests::Expect(result, registry.GetCount() == 0, "ReliableUdpSessionRegistry: initially empty");
		tests::Expect(result, registry.Find(MakeEndpointKey(1, 1000)) == nullptr, "ReliableUdpSessionRegistry: missing endpoint");
	}

	void RunUpsertAndFindTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(1, 1000);
		const common::net::ReliableUdpConfig config{};

		common::net::ReliableUdpSession& insertedSession = registry.Upsert(endpointKey, config);
		common::net::ReliableUdpSession* foundSession = registry.Find(endpointKey);

		tests::Expect(result, registry.GetCount() == 1, "ReliableUdpSessionRegistry: upsert count");
		tests::Expect(result, foundSession != nullptr, "ReliableUdpSessionRegistry: inserted session found");
		tests::Expect(result, foundSession == &insertedSession, "ReliableUdpSessionRegistry: found session matches inserted session");

		const ReliableUdpSessionRegistry& constRegistry = registry;
		const common::net::ReliableUdpSession* constFoundSession = constRegistry.Find(endpointKey);

		tests::Expect(result, constFoundSession != nullptr, "ReliableUdpSessionRegistry: const session found");
		tests::Expect(result, constFoundSession == foundSession, "ReliableUdpSessionRegistry: const find matches mutable find");
	}

	void RunUpsertAppliesConfigTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(2, 2000);

		common::net::ReliableUdpConfig config{};
		config.maxPendingPacketCount = 1;
		config.maxResendCount = 3;
		config.resendInterval = common::time::Milliseconds(150);

		common::net::ReliableUdpSession& session = registry.Upsert(endpointKey, config);

		tests::Expect(result, session.GetMaxResendCount() == 3, "ReliableUdpSessionRegistry: max resend count configured");

		const common::time::TimePoint now = common::time::Clock::now();

		const common::net::ReliableSequence firstSequence = session.AllocateOutgoingSequence();
		const bool firstRegistered = session.RegisterSentPacket(firstSequence, common::packet::PacketBuffer{ 'A' }, now);

		const common::net::ReliableSequence secondSequence = session.AllocateOutgoingSequence();
		const bool secondRegistered = session.RegisterSentPacket(secondSequence, common::packet::PacketBuffer{ 'B' }, now);

		tests::Expect(result, firstRegistered, "ReliableUdpSessionRegistry: configured first pending packet accepted");
		tests::Expect(result, !secondRegistered, "ReliableUdpSessionRegistry: configured max pending packet count applied");

		const common::net::ReliableUdpSession::ResendResult earlyResult =
			session.ExtractResendResult(now + common::time::Milliseconds(149));

		tests::Expect(result, earlyResult.resendPacketList.empty(), "ReliableUdpSessionRegistry: configured resend interval blocks early resend");

		const common::net::ReliableUdpSession::ResendResult resendResult =
			session.ExtractResendResult(now + common::time::Milliseconds(150));

		tests::Expect(result, resendResult.resendPacketList.size() == 1, "ReliableUdpSessionRegistry: configured resend interval applied");
	}

	void RunUpsertExistingSessionResetsStateTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(3, 3000);

		common::net::ReliableUdpConfig firstConfig{};
		firstConfig.maxPendingPacketCount = 4;
		firstConfig.maxResendCount = 5;

		common::net::ReliableUdpSession& firstSession = registry.Upsert(endpointKey, firstConfig);

		const common::time::TimePoint now = common::time::Clock::now();

		const common::net::ReliableSequence firstSequence = firstSession.AllocateOutgoingSequence();
		const bool registered = firstSession.RegisterSentPacket(firstSequence, common::packet::PacketBuffer{ 'A' }, now);

		common::net::ReliableUdpPacketHeader receivedHeader{};
		receivedHeader.sequence = 100;

		const bool isNewPacket = firstSession.ProcessReceivedDataHeader(receivedHeader);

		tests::Expect(result, registered, "ReliableUdpSessionRegistry: pending packet registered before reset");
		tests::Expect(result, isNewPacket, "ReliableUdpSessionRegistry: received sequence registered before reset");
		tests::Expect(result, firstSession.GetPendingPacketCount() == 1, "ReliableUdpSessionRegistry: pending packet exists before reset");
		tests::Expect(result, firstSession.GetNextSequence() == 2, "ReliableUdpSessionRegistry: outgoing sequence advanced before reset");
		tests::Expect(result, firstSession.HasReceivedAnySequence(), "ReliableUdpSessionRegistry: received sequence exists before reset");

		common::net::ReliableUdpConfig secondConfig{};
		secondConfig.maxPendingPacketCount = 2;
		secondConfig.maxResendCount = 7;
		secondConfig.resendInterval = common::time::Milliseconds(200);

		common::net::ReliableUdpSession& resetSession = registry.Upsert(endpointKey, secondConfig);

		tests::Expect(result, registry.GetCount() == 1, "ReliableUdpSessionRegistry: existing upsert keeps count");
		tests::Expect(result, resetSession.GetPendingPacketCount() == 0, "ReliableUdpSessionRegistry: existing upsert clears pending packets");
		tests::Expect(result, resetSession.GetNextSequence() == 1, "ReliableUdpSessionRegistry: existing upsert resets outgoing sequence");
		tests::Expect(result, !resetSession.HasReceivedAnySequence(), "ReliableUdpSessionRegistry: existing upsert resets received sequence");
		tests::Expect(result, resetSession.GetMaxResendCount() == 7, "ReliableUdpSessionRegistry: existing upsert applies new config");
	}

	void RunMultipleSessionTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey firstEndpointKey = MakeEndpointKey(4, 4000);
		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(5, 5000);

		const common::net::ReliableUdpConfig config{};

		common::net::ReliableUdpSession& firstSession = registry.Upsert(firstEndpointKey, config);
		common::net::ReliableUdpSession& secondSession = registry.Upsert(secondEndpointKey, config);

		const common::net::ReliableSequence firstSequence = firstSession.AllocateOutgoingSequence();

		tests::Expect(result, registry.GetCount() == 2, "ReliableUdpSessionRegistry: multiple session count");
		tests::Expect(result, firstSequence == 1, "ReliableUdpSessionRegistry: first session sequence allocated");
		tests::Expect(result, secondSession.GetNextSequence() == 1, "ReliableUdpSessionRegistry: second session state independent");
		tests::Expect(result, registry.Find(firstEndpointKey) == &firstSession, "ReliableUdpSessionRegistry: first session found");
		tests::Expect(result, registry.Find(secondEndpointKey) == &secondSession, "ReliableUdpSessionRegistry: second session found");
	}

	void RunRemoveTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey firstEndpointKey = MakeEndpointKey(6, 6000);
		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(7, 7000);
		const common::net::EndpointKey unknownEndpointKey = MakeEndpointKey(8, 8000);

		const common::net::ReliableUdpConfig config{};

		static_cast<void>(registry.Upsert(firstEndpointKey, config));
		static_cast<void>(registry.Upsert(secondEndpointKey, config));

		const bool firstRemoved = registry.Remove(firstEndpointKey);
		const bool unknownRemoved = registry.Remove(unknownEndpointKey);

		tests::Expect(result, firstRemoved, "ReliableUdpSessionRegistry: existing session removed");
		tests::Expect(result, !unknownRemoved, "ReliableUdpSessionRegistry: unknown session remove rejected");
		tests::Expect(result, registry.GetCount() == 1, "ReliableUdpSessionRegistry: remove count");
		tests::Expect(result, registry.Find(firstEndpointKey) == nullptr, "ReliableUdpSessionRegistry: removed session missing");
		tests::Expect(result, registry.Find(secondEndpointKey) != nullptr, "ReliableUdpSessionRegistry: other session preserved");
	}

	void RunClearTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey firstEndpointKey = MakeEndpointKey(9, 9000);
		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(10, 10000);

		const common::net::ReliableUdpConfig config{};

		static_cast<void>(registry.Upsert(firstEndpointKey, config));
		static_cast<void>(registry.Upsert(secondEndpointKey, config));

		tests::Expect(result, registry.GetCount() == 2, "ReliableUdpSessionRegistry: sessions exist before clear");

		registry.Clear();

		tests::Expect(result, registry.GetCount() == 0, "ReliableUdpSessionRegistry: clear removes all sessions");
		tests::Expect(result, registry.Find(firstEndpointKey) == nullptr, "ReliableUdpSessionRegistry: first session missing after clear");
		tests::Expect(result, registry.Find(secondEndpointKey) == nullptr, "ReliableUdpSessionRegistry: second session missing after clear");
	}

	void RunExtractResendBatchTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(11, 11000);

		common::net::ReliableUdpConfig config{};
		config.maxPendingPacketCount = 2;
		config.maxResendCount = 1;
		config.resendInterval = common::time::Milliseconds(100);

		common::net::ReliableUdpSession& session = registry.Upsert(endpointKey, config);

		const common::time::TimePoint now = common::time::Clock::now();

		const common::net::ReliableSequence sequence = session.AllocateOutgoingSequence();
		const bool registered = session.RegisterSentPacket(sequence, common::packet::PacketBuffer{ 'A', 'B' }, now);

		tests::Expect(result, registered, "ReliableUdpSessionRegistry: resend packet registered");

		const ReliableUdpSessionRegistry::ResendBatch earlyBatch =
			registry.ExtractResendBatch(now + common::time::Milliseconds(99));

		tests::Expect(result, earlyBatch.taskList.empty(), "ReliableUdpSessionRegistry: early resend batch empty");
		tests::Expect(result, earlyBatch.giveUpPacketCount == 0, "ReliableUdpSessionRegistry: early give-up count");

		const ReliableUdpSessionRegistry::ResendBatch resendBatch =
			registry.ExtractResendBatch(now + common::time::Milliseconds(100));

		tests::Expect(result, resendBatch.taskList.size() == 1, "ReliableUdpSessionRegistry: resend task extracted");
		tests::Expect(result, resendBatch.giveUpPacketCount == 0, "ReliableUdpSessionRegistry: resend has no give-up");

		if (!resendBatch.taskList.empty())
		{
			const ReliableUdpSessionRegistry::ResendTask& task = resendBatch.taskList.front();

			tests::Expect(result, task.endpointKey == endpointKey, "ReliableUdpSessionRegistry: resend endpoint");
			tests::Expect(
				result,
				task.packetBuffer.size() == 2 && task.packetBuffer[0] == 'A' && task.packetBuffer[1] == 'B',
				"ReliableUdpSessionRegistry: resend packet buffer"
			);
		}

		const ReliableUdpSessionRegistry::ResendBatch giveUpBatch =
			registry.ExtractResendBatch(now + common::time::Milliseconds(200));

		tests::Expect(result, giveUpBatch.taskList.empty(), "ReliableUdpSessionRegistry: give-up batch has no resend");
		tests::Expect(result, giveUpBatch.giveUpPacketCount == 1, "ReliableUdpSessionRegistry: give-up packet counted");
		tests::Expect(result, session.GetPendingPacketCount() == 0, "ReliableUdpSessionRegistry: give-up removes pending packet");
	}

	void RunProcessReceivedPacketTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(12, 12000);
		const common::net::EndpointKey unknownEndpointKey = MakeEndpointKey(13, 13000);

		static_cast<void>(registry.Upsert(endpointKey, common::net::ReliableUdpConfig{}));

		common::net::ReliableUdpPacketView packetView{};
		packetView.packetHeader.type = common::packet::PacketType::JoinRoomRequest;
		packetView.reliableHeader.sequence = 10;

		const ReliableUdpSessionRegistry::ProcessReceivedPacketResult firstResult =
			registry.ProcessReceivedPacket(endpointKey, packetView);

		tests::Expect(
			result,
			firstResult.status == ReliableUdpSessionRegistry::ProcessReceivedPacketStatus::DataReceived,
			"ReliableUdpSessionRegistry: received packet routed"
		);

		tests::Expect(result, firstResult.ackPacketBuffer.has_value(), "ReliableUdpSessionRegistry: received packet ack built");

		const ReliableUdpSessionRegistry::ProcessReceivedPacketResult duplicateResult =
			registry.ProcessReceivedPacket(endpointKey, packetView);

		tests::Expect(
			result,
			duplicateResult.status == ReliableUdpSessionRegistry::ProcessReceivedPacketStatus::DuplicateData,
			"ReliableUdpSessionRegistry: duplicate packet routed"
		);

		const ReliableUdpSessionRegistry::ProcessReceivedPacketResult unknownResult =
			registry.ProcessReceivedPacket(unknownEndpointKey, packetView);

		tests::Expect(
			result,
			unknownResult.status == ReliableUdpSessionRegistry::ProcessReceivedPacketStatus::SessionNotFound,
			"ReliableUdpSessionRegistry: unknown endpoint rejected"
		);
	}
}

namespace tests::server
{
	DebugTestResult RunReliableUdpSessionRegistryTests()
	{
		DebugTestResult result{};

		RunInitialStateTest(result);
		RunUpsertAndFindTest(result);
		RunUpsertAppliesConfigTest(result);
		RunUpsertExistingSessionResetsStateTest(result);
		RunMultipleSessionTest(result);
		RunRemoveTest(result);
		RunClearTest(result);
		RunExtractResendBatchTest(result);
		RunProcessReceivedPacketTest(result);

		return result;
	}
}