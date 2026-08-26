#include "ControlPacketTests.h"

#include <optional>

#include <Common/Packet/Control/ControlPacket.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/PacketSerialization.h>
#include <Common/Packet/PacketType.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void RunKeepAliveSerializationTest(tests::DebugTestResult& result)
	{
		common::packet::KeepAlivePacket packet{};

		const std::optional<common::packet::PacketBuffer> serializedPacket = common::packet::SerializePacket(packet);

		tests::Expect(result, serializedPacket.has_value(), "ControlPacket: KeepAlive serialize");

		if (!serializedPacket.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			serializedPacket->size() == common::packet::serializedPacketHeaderSize,
			"ControlPacket: KeepAlive header-only serialized size"
		);

		const std::optional<common::packet::KeepAlivePacket> deserializedPacket =
			common::packet::DeserializePacket<common::packet::KeepAlivePacket>(
				serializedPacket->data(),
				static_cast<int>(serializedPacket->size())
			);

		tests::Expect(result, deserializedPacket.has_value(), "ControlPacket: KeepAlive deserialize");

		if (!deserializedPacket.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			deserializedPacket->header.size == common::packet::serializedPacketHeaderSize,
			"ControlPacket: KeepAlive header size"
		);

		tests::Expect(
			result,
			deserializedPacket->header.type == common::packet::PacketType::KeepAlive,
			"ControlPacket: KeepAlive header type"
		);

		tests::Expect(
			result,
			deserializedPacket->header.version == common::packet::protocolVersion,
			"ControlPacket: KeepAlive protocol version"
		);
	}

	void RunKeepAliveWrongTypeTest(tests::DebugTestResult& result)
	{
		common::packet::KeepAlivePacket packet{};

		const std::optional<common::packet::PacketBuffer> serializedPacket = common::packet::SerializePacket(packet);

		tests::Expect(result, serializedPacket.has_value(), "ControlPacket: KeepAlive wrong type base serialize");

		if (!serializedPacket.has_value())
		{
			return;
		}

		const std::optional<common::packet::LeaveRequestPacket> deserializedPacket =
			common::packet::DeserializePacket<common::packet::LeaveRequestPacket>(
				serializedPacket->data(),
				static_cast<int>(serializedPacket->size())
			);

		tests::Expect(
			result,
			!deserializedPacket.has_value(),
			"ControlPacket: KeepAlive cannot deserialize as LeaveRequest"
		);
	}
}

namespace tests::packet
{
	DebugTestResult RunControlPacketTests()
	{
		DebugTestResult result{};

		RunKeepAliveSerializationTest(result);
		RunKeepAliveWrongTypeTest(result);

		return result;
	}
}