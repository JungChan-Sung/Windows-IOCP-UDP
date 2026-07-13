#include "ReliableUdpPacketHeaderTests.h"

#include <optional>

#include <Common/Net/Reliable/ReliableUdpPacketHeader.h>
#include <Common/Net/Reliable/ReliableUdpPacketSerialization.h>

#include <Tests/DebugTestResult.h>

namespace tests::net::reliableUdpPacketHeaderTest
{
	void RunHeaderWireSizeTest(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			common::net::reliableUdpPacketHeaderWireSize == 12,
			"ReliableUdpPacketHeader: wire size"
		);
	}

	void RunHeaderRoundTripTest(tests::DebugTestResult& result)
	{
		common::net::ReliableUdpPacketHeader reliableHeader{};
		reliableHeader.sequence = 100;
		reliableHeader.ackSequence = 90;
		reliableHeader.ackBitfield = 0b1011;

		common::packet::PacketWriter writer;
		writer.Reserve(common::net::reliableUdpPacketHeaderWireSize);
		common::net::WriteReliableUdpPacketHeader(writer, reliableHeader);

		const common::packet::PacketBuffer packetBuffer = writer.TakeBuffer();

		tests::Expect(
			result,
			packetBuffer.size() == common::net::reliableUdpPacketHeaderWireSize,
			"ReliableUdpPacketHeader: serialized size"
		);

		const std::optional<common::net::ReliableUdpPacketHeader> deserializedHeader =
			common::net::DeserializeReliableUdpPacketHeader(
				packetBuffer.data(),
				static_cast<int>(packetBuffer.size())
			);

		tests::Expect(result, deserializedHeader.has_value(), "ReliableUdpPacketHeader: deserialize");

		if (!deserializedHeader.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			deserializedHeader->sequence == reliableHeader.sequence,
			"ReliableUdpPacketHeader: sequence"
		);

		tests::Expect(
			result,
			deserializedHeader->ackSequence == reliableHeader.ackSequence,
			"ReliableUdpPacketHeader: ack sequence"
		);

		tests::Expect(
			result,
			deserializedHeader->ackBitfield == reliableHeader.ackBitfield,
			"ReliableUdpPacketHeader: ack bitfield"
		);
	}

	void RunDeserializeInvalidBufferTest(tests::DebugTestResult& result)
	{
		const std::optional<common::net::ReliableUdpPacketHeader> nullHeader =
			common::net::DeserializeReliableUdpPacketHeader(nullptr, 0);

		tests::Expect(
			result,
			!nullHeader.has_value(),
			"ReliableUdpPacketHeader: null buffer rejected"
		);

		const char shortBuffer[4]{};
		const std::optional<common::net::ReliableUdpPacketHeader> shortHeader =
			common::net::DeserializeReliableUdpPacketHeader(
				shortBuffer,
				static_cast<int>(sizeof(shortBuffer))
			);

		tests::Expect(
			result,
			!shortHeader.has_value(),
			"ReliableUdpPacketHeader: short buffer rejected"
		);
	}
}

namespace tests::net
{
	tests::DebugTestResult RunReliableUdpPacketHeaderTests()
	{
		tests::DebugTestResult result{};

		reliableUdpPacketHeaderTest::RunHeaderWireSizeTest(result);
		reliableUdpPacketHeaderTest::RunHeaderRoundTripTest(result);
		reliableUdpPacketHeaderTest::RunDeserializeInvalidBufferTest(result);

		return result;
	}
}