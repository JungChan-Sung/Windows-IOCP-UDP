#include "ReliableUdpSessionRegistryTests.h"

#include <cstdint>
#include <optional>

#include <Common/Net/Endpoint.h>
#include <Common/Net/Reliable/ReliableUdpConfig.h>
#include <Common/Net/Reliable/ReliableUdpPacketBuilder.h>
#include <Common/Net/Reliable/ReliableUdpPacketHeader.h>
#include <Common/Net/Reliable/ReliableUdpSession.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketBuffer.h>
#include <Common/Packet/PacketSerialization.h>
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
		const common::net::EndpointKey endpointKey = MakeEndpointKey(1, 1000);

		tests::Expect(result, registry.GetCount() == 0, "ReliableUdpSessionRegistry: initially empty");
		tests::Expect(result, registry.Find(endpointKey) == nullptr, "ReliableUdpSessionRegistry: missing endpoint");
		tests::Expect(result, !registry.IsClosing(endpointKey), "ReliableUdpSessionRegistry: missing endpoint is not closing");
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
		tests::Expect(result, !registry.IsClosing(endpointKey), "ReliableUdpSessionRegistry: inserted session is not closing");

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

		const bool beginCloseResult = registry.BeginClose(endpointKey);

		tests::Expect(result, beginCloseResult, "ReliableUdpSessionRegistry: existing session begins close before upsert");
		tests::Expect(result, registry.IsClosing(endpointKey), "ReliableUdpSessionRegistry: session closing before upsert reset");

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
		tests::Expect(result, !registry.IsClosing(endpointKey), "ReliableUdpSessionRegistry: existing upsert clears closing state");
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

		tests::Expect(result, registry.BeginClose(firstEndpointKey), "ReliableUdpSessionRegistry: removed session can enter closing state");
		tests::Expect(result, registry.IsClosing(firstEndpointKey), "ReliableUdpSessionRegistry: removed session closing before remove");

		const bool firstRemoved = registry.Remove(firstEndpointKey);
		const bool unknownRemoved = registry.Remove(unknownEndpointKey);

		tests::Expect(result, firstRemoved, "ReliableUdpSessionRegistry: existing session removed");
		tests::Expect(result, !unknownRemoved, "ReliableUdpSessionRegistry: unknown session remove rejected");
		tests::Expect(result, registry.GetCount() == 1, "ReliableUdpSessionRegistry: remove count");
		tests::Expect(result, registry.Find(firstEndpointKey) == nullptr, "ReliableUdpSessionRegistry: removed session missing");
		tests::Expect(result, registry.Find(secondEndpointKey) != nullptr, "ReliableUdpSessionRegistry: other session preserved");
		tests::Expect(result, !registry.IsClosing(firstEndpointKey), "ReliableUdpSessionRegistry: remove clears closing state");
	}

	void RunClearTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey firstEndpointKey = MakeEndpointKey(9, 9000);
		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(10, 10000);

		const common::net::ReliableUdpConfig config{};

		static_cast<void>(registry.Upsert(firstEndpointKey, config));
		static_cast<void>(registry.Upsert(secondEndpointKey, config));
		static_cast<void>(registry.BeginClose(firstEndpointKey));
		static_cast<void>(registry.BeginClose(secondEndpointKey));

		tests::Expect(result, registry.GetCount() == 2, "ReliableUdpSessionRegistry: sessions exist before clear");
		tests::Expect(result, registry.IsClosing(firstEndpointKey), "ReliableUdpSessionRegistry: first session closing before clear");
		tests::Expect(result, registry.IsClosing(secondEndpointKey), "ReliableUdpSessionRegistry: second session closing before clear");

		registry.Clear();

		tests::Expect(result, registry.GetCount() == 0, "ReliableUdpSessionRegistry: clear removes all sessions");
		tests::Expect(result, registry.Find(firstEndpointKey) == nullptr, "ReliableUdpSessionRegistry: first session missing after clear");
		tests::Expect(result, registry.Find(secondEndpointKey) == nullptr, "ReliableUdpSessionRegistry: second session missing after clear");
		tests::Expect(result, !registry.IsClosing(firstEndpointKey), "ReliableUdpSessionRegistry: clear removes first closing state");
		tests::Expect(result, !registry.IsClosing(secondEndpointKey), "ReliableUdpSessionRegistry: clear removes second closing state");
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

	void RunPendingPacketCountTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey firstEndpointKey = MakeEndpointKey(14, 14000);
		const common::net::EndpointKey secondEndpointKey = MakeEndpointKey(15, 15000);

		const common::net::ReliableUdpConfig config{};
		const common::time::TimePoint now = common::time::Clock::now();

		common::net::ReliableUdpSession& firstSession = registry.Upsert(firstEndpointKey, config);
		common::net::ReliableUdpSession& secondSession = registry.Upsert(secondEndpointKey, config);

		const bool firstRegistered = firstSession.RegisterSentPacket(
			firstSession.AllocateOutgoingSequence(),
			common::packet::PacketBuffer{ 'A' },
			now
		);

		const bool secondRegistered = firstSession.RegisterSentPacket(
			firstSession.AllocateOutgoingSequence(),
			common::packet::PacketBuffer{ 'B' },
			now
		);

		const bool thirdRegistered = secondSession.RegisterSentPacket(
			secondSession.AllocateOutgoingSequence(),
			common::packet::PacketBuffer{ 'C' },
			now
		);

		tests::Expect(result, firstRegistered && secondRegistered && thirdRegistered, "ReliableUdpSessionRegistry: pending packets registered");
		tests::Expect(result, registry.GetPendingPacketCount() == 3, "ReliableUdpSessionRegistry: pending packet count aggregated");

		static_cast<void>(registry.Remove(firstEndpointKey));

		tests::Expect(result, registry.GetPendingPacketCount() == 1, "ReliableUdpSessionRegistry: pending packet count updated after remove");

		registry.Clear();

		tests::Expect(result, registry.GetPendingPacketCount() == 0, "ReliableUdpSessionRegistry: pending packet count cleared");
	}

	void RunBeginCloseUnknownSessionTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(16, 16000);

		const bool beginCloseResult = registry.BeginClose(endpointKey);

		tests::Expect(result, !beginCloseResult, "ReliableUdpSessionRegistry: unknown session cannot begin close");
		tests::Expect(result, !registry.IsClosing(endpointKey), "ReliableUdpSessionRegistry: unknown session remains not closing");
		tests::Expect(result, registry.GetCount() == 0, "ReliableUdpSessionRegistry: unknown close does not create session");
	}

	void RunClosingSessionRemovedAfterAckTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(17, 17000);
		const common::time::TimePoint now = common::time::Clock::now();

		static_cast<void>(registry.Upsert(endpointKey, common::net::ReliableUdpConfig{}));

		common::packet::LeaveResponsePacket leaveResponsePacket{};

		const std::optional<common::packet::PacketBuffer> serializedPacket =
			common::packet::SerializePacket(leaveResponsePacket);

		tests::Expect(result, serializedPacket.has_value(), "ReliableUdpSessionRegistry: serialize leave response");

		if (!serializedPacket.has_value())
		{
			return;
		}

		ReliableUdpSessionRegistry::BuildOutgoingPacketResult buildResult =
			registry.BuildOutgoingPacket(
				endpointKey,
				common::packet::ConstPacketSpan(serializedPacket->data(), serializedPacket->size()),
				now
			);

		tests::Expect(result, buildResult.has_value(), "ReliableUdpSessionRegistry: build reliable leave response");

		if (!buildResult.has_value())
		{
			return;
		}

		const std::optional<common::net::ReliableUdpPacketView> leaveResponseView =
			common::net::ParseReliableUdpPacket(buildResult->data(), static_cast<int>(buildResult->size()));

		tests::Expect(result, leaveResponseView.has_value(), "ReliableUdpSessionRegistry: parse reliable leave response");

		if (!leaveResponseView.has_value())
		{
			return;
		}

		tests::Expect(result, registry.GetPendingPacketCount() == 1, "ReliableUdpSessionRegistry: leave response pending before close");

		const bool beginCloseResult = registry.BeginClose(endpointKey);

		tests::Expect(result, beginCloseResult, "ReliableUdpSessionRegistry: begin close");
		tests::Expect(result, registry.IsClosing(endpointKey), "ReliableUdpSessionRegistry: session marked closing");
		tests::Expect(result, registry.GetCount() == 1, "ReliableUdpSessionRegistry: closing session retained before ack");

		common::net::ReliableUdpPacketView ackPacketView{};
		ackPacketView.packetHeader.type = common::packet::PacketType::None;
		ackPacketView.reliableHeader.ackSequence = leaveResponseView->reliableHeader.sequence;

		const ReliableUdpSessionRegistry::ProcessReceivedPacketResult processResult =
			registry.ProcessReceivedPacket(endpointKey, ackPacketView);

		tests::Expect(
			result,
			processResult.status == ReliableUdpSessionRegistry::ProcessReceivedPacketStatus::AckOnlyProcessed,
			"ReliableUdpSessionRegistry: closing ack processed"
		);

		tests::Expect(result, registry.GetCount() == 0, "ReliableUdpSessionRegistry: closing session removed after ack");
		tests::Expect(result, registry.GetPendingPacketCount() == 0, "ReliableUdpSessionRegistry: no pending packet after closing ack");
		tests::Expect(result, registry.Find(endpointKey) == nullptr, "ReliableUdpSessionRegistry: closed session no longer found");
		tests::Expect(result, !registry.IsClosing(endpointKey), "ReliableUdpSessionRegistry: closing marker removed after ack");
	}

	void RunClosingSessionRemovedAfterGiveUpTest(tests::DebugTestResult& result)
	{
		ReliableUdpSessionRegistry registry;

		const common::net::EndpointKey endpointKey = MakeEndpointKey(18, 18000);

		common::net::ReliableUdpConfig config{};
		config.maxPendingPacketCount = 1;
		config.maxResendCount = 0;
		config.resendInterval = common::time::Milliseconds(100);

		static_cast<void>(registry.Upsert(endpointKey, config));

		common::packet::LeaveResponsePacket leaveResponsePacket{};

		const std::optional<common::packet::PacketBuffer> serializedPacket =
			common::packet::SerializePacket(leaveResponsePacket);

		tests::Expect(result, serializedPacket.has_value(), "ReliableUdpSessionRegistry: serialize closing give-up packet");

		if (!serializedPacket.has_value())
		{
			return;
		}

		const common::time::TimePoint now = common::time::Clock::now();

		const ReliableUdpSessionRegistry::BuildOutgoingPacketResult buildResult =
			registry.BuildOutgoingPacket(
				endpointKey,
				common::packet::ConstPacketSpan(serializedPacket->data(), serializedPacket->size()),
				now
			);

		tests::Expect(result, buildResult.has_value(), "ReliableUdpSessionRegistry: build closing give-up packet");

		if (!buildResult.has_value())
		{
			return;
		}

		tests::Expect(result, registry.GetPendingPacketCount() == 1, "ReliableUdpSessionRegistry: closing give-up packet pending");
		tests::Expect(result, registry.BeginClose(endpointKey), "ReliableUdpSessionRegistry: begin close before give-up");
		tests::Expect(result, registry.IsClosing(endpointKey), "ReliableUdpSessionRegistry: closing before give-up");

		const ReliableUdpSessionRegistry::ResendBatch earlyBatch =
			registry.ExtractResendBatch(now + common::time::Milliseconds(99));

		tests::Expect(result, earlyBatch.taskList.empty(), "ReliableUdpSessionRegistry: closing packet not resent early");
		tests::Expect(result, earlyBatch.giveUpPacketCount == 0, "ReliableUdpSessionRegistry: closing packet not given up early");
		tests::Expect(result, registry.GetCount() == 1, "ReliableUdpSessionRegistry: closing session retained before give-up");

		const ReliableUdpSessionRegistry::ResendBatch giveUpBatch =
			registry.ExtractResendBatch(now + common::time::Milliseconds(100));

		tests::Expect(result, giveUpBatch.taskList.empty(), "ReliableUdpSessionRegistry: closing give-up has no resend task");
		tests::Expect(result, giveUpBatch.giveUpPacketCount == 1, "ReliableUdpSessionRegistry: closing packet give-up counted");
		tests::Expect(result, registry.GetCount() == 0, "ReliableUdpSessionRegistry: closing session removed after give-up");
		tests::Expect(result, registry.GetPendingPacketCount() == 0, "ReliableUdpSessionRegistry: closing pending packet removed after give-up");
		tests::Expect(result, registry.Find(endpointKey) == nullptr, "ReliableUdpSessionRegistry: give-up session no longer found");
		tests::Expect(result, !registry.IsClosing(endpointKey), "ReliableUdpSessionRegistry: closing marker removed after give-up");
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
		RunPendingPacketCountTest(result);

		RunBeginCloseUnknownSessionTest(result);
		RunClosingSessionRemovedAfterAckTest(result);
		RunClosingSessionRemovedAfterGiveUpTest(result);

		return result;
	}
}