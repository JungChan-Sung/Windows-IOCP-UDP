#include "PacketReliabilityTests.h"

#include <array>

#include <Common/Packet/PacketReliability.h>
#include <Common/Packet/PacketType.h>

#include <Tests/DebugTestResult.h>

namespace tests::packet::packetReliabilityTest
{
	inline constexpr std::array reliablePacketTypes
	{
		common::packet::PacketType::LeaveRequest,
		common::packet::PacketType::JoinRoomRequest,
		common::packet::PacketType::JoinRoomResponse,
	};

	inline constexpr std::array unreliablePacketTypes
	{
		common::packet::PacketType::JoinRequest,
		common::packet::PacketType::JoinResponse,
		common::packet::PacketType::InputCommand,
		common::packet::PacketType::FireRequest,
		common::packet::PacketType::PlayerJoined,
		common::packet::PacketType::PlayerLeft,
		common::packet::PacketType::PlayerSnapshot,
		common::packet::PacketType::BulletSnapshot,
		common::packet::PacketType::ImpactEffect,
	};

	void RunReliablePacketTypeTests(tests::DebugTestResult& result)
	{
		for (const common::packet::PacketType packetType : reliablePacketTypes)
		{
			tests::Expect(
				result,
				common::packet::IsReliablePacketType(packetType),
				"PacketReliability: reliable packet type"
			);
		}
	}

	void RunUnreliablePacketTypeTests(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			!common::packet::IsReliablePacketType(common::packet::PacketType::None),
			"PacketReliability: none is not reliable game packet type"
		);

		for (const common::packet::PacketType packetType : unreliablePacketTypes)
		{
			tests::Expect(
				result,
				!common::packet::IsReliablePacketType(packetType),
				"PacketReliability: unreliable packet type"
			);
		}
	}

	void RunReliableTransportValidationTests(tests::DebugTestResult& result)
	{
		for (const common::packet::PacketType packetType : reliablePacketTypes)
		{
			tests::Expect(
				result,
				common::packet::IsPacketTransportReliabilityValid(packetType, true),
				"PacketReliability: reliable packet accepts reliable transport"
			);

			tests::Expect(
				result,
				!common::packet::IsPacketTransportReliabilityValid(packetType, false),
				"PacketReliability: reliable packet rejects unreliable transport"
			);
		}
	}

	void RunUnreliableTransportValidationTests(tests::DebugTestResult& result)
	{
		for (const common::packet::PacketType packetType : unreliablePacketTypes)
		{
			tests::Expect(
				result,
				common::packet::IsPacketTransportReliabilityValid(packetType, false),
				"PacketReliability: unreliable packet accepts unreliable transport"
			);

			tests::Expect(
				result,
				!common::packet::IsPacketTransportReliabilityValid(packetType, true),
				"PacketReliability: unreliable packet rejects reliable transport"
			);
		}
	}

	void RunAckTransportValidationTests(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			common::packet::IsPacketTransportReliabilityValid(common::packet::PacketType::None, true),
			"PacketReliability: ack accepts reliable transport"
		);

		tests::Expect(
			result,
			!common::packet::IsPacketTransportReliabilityValid(common::packet::PacketType::None, false),
			"PacketReliability: ack rejects unreliable transport"
		);
	}
}

namespace tests::packet
{
	tests::DebugTestResult RunPacketReliabilityTests()
	{
		tests::DebugTestResult result{};

		packetReliabilityTest::RunReliablePacketTypeTests(result);
		packetReliabilityTest::RunUnreliablePacketTypeTests(result);
		packetReliabilityTest::RunReliableTransportValidationTests(result);
		packetReliabilityTest::RunUnreliableTransportValidationTests(result);
		packetReliabilityTest::RunAckTransportValidationTests(result);

		return result;
	}
}