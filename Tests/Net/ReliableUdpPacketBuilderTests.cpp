#include "ReliableUdpPacketBuilderTests.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <Common/Net/Reliable/ReliableUdpPacketBuilder.h>
#include <Common/Net/Reliable/ReliableUdpPacketSerialization.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Packet/Serialization/PacketSerializationCore.h>

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

		const std::optional<common::packet::PacketBuffer> reliablePacket = common::net::BuildReliableUdpPacket(
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

		const std::optional<common::net::ReliableUdpPacketView> packetView =
			common::net::ParseReliableUdpPacket(
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
			common::net::BuildGamePacketFromReliableUdpPacketView(*packetView);

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

	void RunBuildAndParseHeaderOnlyReliablePacketTest(tests::DebugTestResult& result)
	{
		common::packet::LeaveRequestPacket gamePacket{};

		const std::optional<common::packet::PacketBuffer> serializedGamePacket = common::packet::SerializePacket(gamePacket);

		tests::Expect(result, serializedGamePacket.has_value(), "ReliableUdpPacketBuilder: serialize header-only game packet");

		if (!serializedGamePacket.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			serializedGamePacket->size() == common::packet::serializedPacketHeaderSize,
			"ReliableUdpPacketBuilder: header-only game packet size"
		);

		common::net::ReliableUdpPacketHeader reliableHeader{};
		reliableHeader.sequence = 20;
		reliableHeader.ackSequence = 8;
		reliableHeader.ackBitfield = 0b11;

		const std::optional<common::packet::PacketBuffer> reliablePacket = common::net::BuildReliableUdpPacket(
			reliableHeader,
			std::span<const char>(serializedGamePacket->data(), serializedGamePacket->size())
		);

		tests::Expect(result, reliablePacket.has_value(), "ReliableUdpPacketBuilder: build header-only reliable packet");

		if (!reliablePacket.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			reliablePacket->size() == common::net::reliableUdpPayloadOffset,
			"ReliableUdpPacketBuilder: header-only reliable packet size"
		);

		const std::optional<common::net::ReliableUdpPacketView> packetView =
			common::net::ParseReliableUdpPacket(
				reliablePacket->data(),
				static_cast<int>(reliablePacket->size())
			);

		tests::Expect(result, packetView.has_value(), "ReliableUdpPacketBuilder: parse header-only reliable packet");

		if (!packetView.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			packetView->packetHeader.type == common::packet::PacketType::LeaveRequest,
			"ReliableUdpPacketBuilder: header-only reliable packet type"
		);

		tests::Expect(result, packetView->payload.empty(), "ReliableUdpPacketBuilder: header-only reliable payload empty");
		tests::Expect(result, packetView->reliableHeader.sequence == reliableHeader.sequence,
			"ReliableUdpPacketBuilder: header-only reliable sequence");

		const std::optional<common::packet::PacketBuffer> rebuiltGamePacket =
			common::net::BuildGamePacketFromReliableUdpPacketView(*packetView);

		tests::Expect(result, rebuiltGamePacket.has_value(), "ReliableUdpPacketBuilder: rebuild header-only game packet");

		if (!rebuiltGamePacket.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			rebuiltGamePacket->size() == common::packet::serializedPacketHeaderSize,
			"ReliableUdpPacketBuilder: rebuilt header-only game packet size"
		);

		const std::optional<common::packet::LeaveRequestPacket> parsedGamePacket =
			common::packet::DeserializePacket<common::packet::LeaveRequestPacket>(
				rebuiltGamePacket->data(),
				static_cast<int>(rebuiltGamePacket->size())
			);

		tests::Expect(result, parsedGamePacket.has_value(), "ReliableUdpPacketBuilder: deserialize rebuilt header-only game packet");
	}

	void RunRejectInvalidBuildInputTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpPacketHeader reliableHeader{};

		const std::optional<common::packet::PacketBuffer> emptyPacket =
			common::net::BuildReliableUdpPacket(reliableHeader, std::span<const char>{});

		tests::Expect(result, !emptyPacket.has_value(), "ReliableUdpPacketBuilder: reject empty game packet");
	}

	void RunRejectInvalidParseBufferTest(tests::DebugTestResult& result)
	{
		const std::optional<common::net::ReliableUdpPacketView> nullPacket =
			common::net::ParseReliableUdpPacket(nullptr, 0);

		tests::Expect(result, !nullPacket.has_value(), "ReliableUdpPacketBuilder: reject null packet");

		char invalidPacket[common::net::reliableUdpPayloadOffset]{};

		const std::optional<common::net::ReliableUdpPacketView> invalidPacketView =
			common::net::ParseReliableUdpPacket(
				invalidPacket,
				static_cast<int>(sizeof(invalidPacket))
			);

		tests::Expect(result, !invalidPacketView.has_value(), "ReliableUdpPacketBuilder: reject invalid packet");
	}

	void RunRejectUnreliablePacketTypeTest(tests::DebugTestResult& result)
	{
		common::packet::JoinRequestPacket gamePacket{};

		const std::optional<common::packet::PacketBuffer> serializedGamePacket = common::packet::SerializePacket(gamePacket);

		tests::Expect(result, serializedGamePacket.has_value(), "ReliableUdpPacketBuilder: serialize unreliable game packet");

		if (!serializedGamePacket.has_value())
		{
			return;
		}

		common::net::ReliableUdpPacketHeader reliableHeader{};

		const std::optional<common::packet::PacketBuffer> reliablePacket = common::net::BuildReliableUdpPacket(
			reliableHeader,
			std::span<const char>(serializedGamePacket->data(), serializedGamePacket->size())
		);

		tests::Expect(
			result,
			!reliablePacket.has_value(),
			"ReliableUdpPacketBuilder: reject unreliable game packet type"
		);

		const std::size_t malformedPacketSize = common::net::reliableUdpPayloadOffset;

		common::packet::PacketWriter writer;
		writer.Reserve(malformedPacketSize);

		common::packet::WritePacketHeader(
			writer,
			static_cast<std::uint16_t>(malformedPacketSize),
			common::packet::PacketType::JoinRequest,
			true
		);

		common::net::WriteReliableUdpPacketHeader(writer, reliableHeader);

		const common::packet::PacketBuffer malformedPacket = writer.TakeBuffer();

		const std::optional<common::net::ReliableUdpPacketView> packetView =
			common::net::ParseReliableUdpPacket(
				malformedPacket.data(),
				static_cast<int>(malformedPacket.size())
			);

		tests::Expect(
			result,
			!packetView.has_value(),
			"ReliableUdpPacketBuilder: reject reliable wrapper with unreliable packet type"
		);
	}

	void RunRejectAckOnlyPacketWithPayloadTest(tests::DebugTestResult& result)
	{
		const std::size_t packetSize = common::net::reliableUdpPayloadOffset + 1;

		common::packet::PacketWriter writer;
		writer.Reserve(packetSize);

		common::packet::WritePacketHeader(
			writer,
			static_cast<std::uint16_t>(packetSize),
			common::packet::PacketType::None,
			true
		);

		common::net::WriteReliableUdpPacketHeader(writer, common::net::ReliableUdpPacketHeader{});
		writer.WriteUInt8(1);

		const common::packet::PacketBuffer packetBuffer = writer.TakeBuffer();

		const std::optional<common::net::ReliableUdpPacketView> packetView =
			common::net::ParseReliableUdpPacket(
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(
			result,
			!packetView.has_value(),
			"ReliableUdpPacketBuilder: reject ack-only packet with payload"
		);
	}

	void RunBuildAndParseAckOnlyPacketTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpPacketHeader reliableHeader{};
		reliableHeader.ackSequence = 10;
		reliableHeader.ackBitfield = 0b101;

		const std::optional<common::packet::PacketBuffer> ackPacket =
			common::net::BuildReliableUdpAckPacket(reliableHeader);

		tests::Expect(result, ackPacket.has_value(), "ReliableUdpPacketBuilder: build ack-only packet");

		if (!ackPacket.has_value())
		{
			return;
		}

		const std::optional<common::net::ReliableUdpPacketView> packetView =
			common::net::ParseReliableUdpPacket(
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
			common::net::BuildGamePacketFromReliableUdpPacketView(*packetView);

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
		reliableUdpPacketBuilderTest::RunBuildAndParseHeaderOnlyReliablePacketTest(result);
		reliableUdpPacketBuilderTest::RunRejectInvalidBuildInputTest(result);
		reliableUdpPacketBuilderTest::RunRejectInvalidParseBufferTest(result);
		reliableUdpPacketBuilderTest::RunRejectUnreliablePacketTypeTest(result);
		reliableUdpPacketBuilderTest::RunRejectAckOnlyPacketWithPayloadTest(result);
		reliableUdpPacketBuilderTest::RunBuildAndParseAckOnlyPacketTest(result);

		return result;
	}
}