#include "ReliableUdpPacketBuilderTests.h"

#include <optional>
#include <vector>

#include <Common/Packet/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Packet/ReliableUdpPacketBuilder.h>

#include <Tests/DebugTestResult.h>

namespace tests::net::reliableUdpPacketBuilderTest
{
	void RunBuildAndParseReliablePacketTest(tests::DebugTestResult& result)
	{
		common::packet::JoinRoomRequestPacket gamePacket{};
		gamePacket.roomId = 3;

		const std::optional<common::packet::PacketBuffer> payload =
			common::packet::SerializePacket(gamePacket);

		tests::Expect(result, payload.has_value(), "ReliableUdpPacketBuilder: payload serialize");

		if (!payload.has_value())
		{
			return;
		}

		common::net::ReliableUdpPacketHeader reliableHeader{};
		reliableHeader.sequence = 10;
		reliableHeader.ackSequence = 7;
		reliableHeader.ackBitfield = 0b101;

		const std::optional<common::packet::PacketBuffer> reliablePacket =
			common::packet::BuildReliableUdpPacket(reliableHeader, *payload);

		tests::Expect(result, reliablePacket.has_value(), "ReliableUdpPacketBuilder: build packet");

		if (!reliablePacket.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			reliablePacket->size() == common::net::reliableUdpPacketHeaderWireSize + payload->size(),
			"ReliableUdpPacketBuilder: packet size"
		);

		const std::optional<common::packet::ReliableUdpPacketView> packetView =
			common::packet::ParseReliableUdpPacket(
				reliablePacket->data(),
				static_cast<int>(reliablePacket->size())
			);

		tests::Expect(result, packetView.has_value(), "ReliableUdpPacketBuilder: parse packet");

		if (!packetView.has_value())
		{
			return;
		}

		tests::Expect(result, packetView->reliableHeader.sequence == reliableHeader.sequence, "ReliableUdpPacketBuilder: sequence");
		tests::Expect(result, packetView->reliableHeader.ackSequence == reliableHeader.ackSequence, "ReliableUdpPacketBuilder: ack sequence");
		tests::Expect(result, packetView->reliableHeader.ackBitfield == reliableHeader.ackBitfield, "ReliableUdpPacketBuilder: ack bitfield");

		const std::optional<common::packet::JoinRoomRequestPacket> parsedPayload =
			common::packet::DeserializePacket<common::packet::JoinRoomRequestPacket>(
				packetView->payload.data(),
				static_cast<int>(packetView->payload.size())
			);

		tests::Expect(result, parsedPayload.has_value(), "ReliableUdpPacketBuilder: deserialize payload");

		if (parsedPayload.has_value())
		{
			tests::Expect(result, parsedPayload->roomId == gamePacket.roomId, "ReliableUdpPacketBuilder: payload room id");
		}
	}

	void RunRejectEmptyPayloadTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpPacketHeader reliableHeader{};

		const std::optional<common::packet::PacketBuffer> reliablePacket =
			common::packet::BuildReliableUdpPacket(reliableHeader, std::span<const char>{});

		tests::Expect(result, !reliablePacket.has_value(), "ReliableUdpPacketBuilder: reject empty payload");
	}

	void RunRejectInvalidParseBufferTest(tests::DebugTestResult& result)
	{
		const std::optional<common::packet::ReliableUdpPacketView> nullPacket =
			common::packet::ParseReliableUdpPacket(nullptr, 0);

		tests::Expect(result, !nullPacket.has_value(), "ReliableUdpPacketBuilder: reject null packet");

		char headerOnlyPacket[common::net::reliableUdpPacketHeaderWireSize]{};

		const std::optional<common::packet::ReliableUdpPacketView> headerOnlyPacketView =
			common::packet::ParseReliableUdpPacket(
				headerOnlyPacket,
				static_cast<int>(sizeof(headerOnlyPacket))
			);

		tests::Expect(result, !headerOnlyPacketView.has_value(), "ReliableUdpPacketBuilder: reject header only packet");
	}
}

namespace tests::net
{
	tests::DebugTestResult RunReliableUdpPacketBuilderTests()
	{
		tests::DebugTestResult result{};

		reliableUdpPacketBuilderTest::RunBuildAndParseReliablePacketTest(result);
		reliableUdpPacketBuilderTest::RunRejectEmptyPayloadTest(result);
		reliableUdpPacketBuilderTest::RunRejectInvalidParseBufferTest(result);

		return result;
	}
}