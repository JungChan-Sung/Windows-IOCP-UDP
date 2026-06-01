#include "ReliableUdpPacketBuilderTests.h"

#include <optional>
#include <span>

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

		const std::optional<common::packet::PacketBuffer> serializedGamePacket = common::packet::SerializePacket(gamePacket);

		tests::Expect(result, serializedGamePacket.has_value(), "ReliableUdpPacketBuilder: serialize game packet");

		if (!serializedGamePacket.has_value())
		{
			return;
		}

		common::net::ReliableUdpPacketHeader reliableHeader{};
		reliableHeader.sequence = 10;
		reliableHeader.ackSequence = 7;
		reliableHeader.ackBitfield = 0b101;

		const std::optional<common::packet::PacketBuffer> reliablePacket =
			common::packet::BuildReliableUdpPacket(
				reliableHeader,
				std::span<const char>(serializedGamePacket->data(), serializedGamePacket->size())
			);

		tests::Expect(result, reliablePacket.has_value(), "ReliableUdpPacketBuilder: build reliable packet");

		if (!reliablePacket.has_value())
		{
			return;
		}

		const std::optional<common::packet::JoinRoomRequestPacket> directlyParsedReliablePacket =
			common::packet::DeserializePacket<common::packet::JoinRoomRequestPacket>(
				reliablePacket->data(),
				static_cast<int>(reliablePacket->size())
			);

		tests::Expect(
			result,
			!directlyParsedReliablePacket.has_value(),
			"ReliableUdpPacketBuilder: reliable packet cannot be deserialized directly"
		);

		const std::size_t expectedReliablePacketSize =
			common::packet::serializedPacketHeaderSize
			+ common::net::reliableUdpPacketHeaderWireSize
			+ serializedGamePacket->size()
			- common::packet::serializedPacketHeaderSize;

		tests::Expect(
			result,
			reliablePacket->size() == expectedReliablePacketSize,
			"ReliableUdpPacketBuilder: reliable packet size"
		);

		const std::optional<common::packet::PacketHeader> reliablePacketHeader =
			common::packet::DeserializePacketHeader(
				reliablePacket->data(),
				static_cast<int>(reliablePacket->size())
			);

		tests::Expect(result, reliablePacketHeader.has_value(), "ReliableUdpPacketBuilder: reliable packet header");

		if (reliablePacketHeader.has_value())
		{
			tests::Expect(
				result,
				reliablePacketHeader->type == common::packet::PacketType::JoinRoomRequest,
				"ReliableUdpPacketBuilder: reliable packet type"
			);

			tests::Expect(
				result,
				common::packet::IsReliablePacketHeader(*reliablePacketHeader),
				"ReliableUdpPacketBuilder: reliable flag"
			);

			tests::Expect(
				result,
				common::packet::GetPacketHeaderProtocolVersion(*reliablePacketHeader) == common::packet::protocolVersion,
				"ReliableUdpPacketBuilder: reliable packet protocol version"
			);
		}

		const std::optional<common::packet::ReliableUdpPacketView> packetView =
			common::packet::ParseReliableUdpPacket(
				reliablePacket->data(),
				static_cast<int>(reliablePacket->size())
			);

		tests::Expect(result, packetView.has_value(), "ReliableUdpPacketBuilder: parse reliable packet");

		if (!packetView.has_value())
		{
			return;
		}

		tests::Expect(result, packetView->reliableHeader.sequence == reliableHeader.sequence, "ReliableUdpPacketBuilder: sequence");
		tests::Expect(result, packetView->reliableHeader.ackSequence == reliableHeader.ackSequence, "ReliableUdpPacketBuilder: ack sequence");
		tests::Expect(result, packetView->reliableHeader.ackBitfield == reliableHeader.ackBitfield, "ReliableUdpPacketBuilder: ack bitfield");

		const std::optional<common::packet::PacketBuffer> rebuiltGamePacket =
			common::packet::BuildGamePacketFromReliableUdpPacketView(*packetView);

		tests::Expect(result, rebuiltGamePacket.has_value(), "ReliableUdpPacketBuilder: rebuild game packet");

		if (!rebuiltGamePacket.has_value())
		{
			return;
		}

		const std::optional<common::packet::JoinRoomRequestPacket> parsedGamePacket =
			common::packet::DeserializePacket<common::packet::JoinRoomRequestPacket>(
				rebuiltGamePacket->data(),
				static_cast<int>(rebuiltGamePacket->size())
			);

		tests::Expect(result, parsedGamePacket.has_value(), "ReliableUdpPacketBuilder: deserialize rebuilt game packet");

		if (parsedGamePacket.has_value())
		{
			tests::Expect(result, parsedGamePacket->roomId == gamePacket.roomId, "ReliableUdpPacketBuilder: room id");
		}
	}

	void RunRejectInvalidBuildInputTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpPacketHeader reliableHeader{};

		const std::optional<common::packet::PacketBuffer> emptyPacket =
			common::packet::BuildReliableUdpPacket(reliableHeader, std::span<const char>{});

		tests::Expect(result, !emptyPacket.has_value(), "ReliableUdpPacketBuilder: reject empty game packet");
	}

	void RunRejectInvalidParseBufferTest(tests::DebugTestResult& result)
	{
		const std::optional<common::packet::ReliableUdpPacketView> nullPacket =
			common::packet::ParseReliableUdpPacket(nullptr, 0);

		tests::Expect(result, !nullPacket.has_value(), "ReliableUdpPacketBuilder: reject null packet");

		char headerOnlyPacket[common::packet::reliableUdpPayloadOffset]{};

		const std::optional<common::packet::ReliableUdpPacketView> headerOnlyPacketView =
			common::packet::ParseReliableUdpPacket(
				headerOnlyPacket,
				static_cast<int>(sizeof(headerOnlyPacket))
			);

		tests::Expect(result, !headerOnlyPacketView.has_value(), "ReliableUdpPacketBuilder: reject header only packet");
	}

	void RunBuildAndParseAckOnlyPacketTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpPacketHeader reliableHeader{};
		reliableHeader.ackSequence = 10;
		reliableHeader.ackBitfield = 0b101;

		const std::optional<common::packet::PacketBuffer> ackPacket =
			common::packet::BuildReliableUdpAckPacket(reliableHeader);

		tests::Expect(result, ackPacket.has_value(), "ReliableUdpPacketBuilder: build ack-only packet");

		if (!ackPacket.has_value())
		{
			return;
		}

		const std::optional<common::packet::ReliableUdpPacketView> packetView =
			common::packet::ParseReliableUdpPacket(
				ackPacket->data(),
				static_cast<int>(ackPacket->size())
			);

		tests::Expect(result, packetView.has_value(), "ReliableUdpPacketBuilder: parse ack-only packet");

		if (!packetView.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			packetView->packetHeader.type == common::packet::PacketType::None,
			"ReliableUdpPacketBuilder: ack-only packet type"
		);

		tests::Expect(
			result,
			common::packet::IsReliablePacketHeader(packetView->packetHeader),
			"ReliableUdpPacketBuilder: ack-only reliable flag"
		);

		tests::Expect(result, packetView->payload.empty(), "ReliableUdpPacketBuilder: ack-only payload empty");
		tests::Expect(result, packetView->reliableHeader.ackSequence == reliableHeader.ackSequence, "ReliableUdpPacketBuilder: ack sequence");
		tests::Expect(result, packetView->reliableHeader.ackBitfield == reliableHeader.ackBitfield, "ReliableUdpPacketBuilder: ack bitfield");

		const std::optional<common::packet::PacketBuffer> gamePacketBuffer =
			common::packet::BuildGamePacketFromReliableUdpPacketView(*packetView);

		tests::Expect(
			result,
			!gamePacketBuffer.has_value(),
			"ReliableUdpPacketBuilder: ack-only cannot rebuild game packet"
		);
	}
}

namespace tests::net
{
	tests::DebugTestResult RunReliableUdpPacketBuilderTests()
	{
		tests::DebugTestResult result{};

		reliableUdpPacketBuilderTest::RunBuildAndParseReliablePacketTest(result);
		reliableUdpPacketBuilderTest::RunRejectInvalidBuildInputTest(result);
		reliableUdpPacketBuilderTest::RunRejectInvalidParseBufferTest(result);
		reliableUdpPacketBuilderTest::RunBuildAndParseAckOnlyPacketTest(result);

		return result;
	}
}