#include "PacketReliabilityTests.h"

#include <array>

#include <Common/Packet/PacketReliability.h>
#include <Common/Packet/PacketType.h>

#include <Tests/DebugTestResult.h>

namespace tests::packet::packetReliabilityTest
{
	void RunReliablePacketTypeTests(tests::DebugTestResult& result)
	{
		constexpr std::array reliablePacketTypes
		{
			common::packet::PacketType::LeaveRequest,
			common::packet::PacketType::JoinRoomRequest,
			common::packet::PacketType::JoinRoomResponse,
		};

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
		constexpr std::array unreliablePacketTypes
		{
			common::packet::PacketType::None,
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

		for (const common::packet::PacketType packetType : unreliablePacketTypes)
		{
			tests::Expect(
				result,
				!common::packet::IsReliablePacketType(packetType),
				"PacketReliability: unreliable packet type"
			);
		}
	}
}

namespace tests::packet
{
	tests::DebugTestResult RunPacketReliabilityTests()
	{
		tests::DebugTestResult result{};

		packetReliabilityTest::RunReliablePacketTypeTests(result);
		packetReliabilityTest::RunUnreliablePacketTypeTests(result);

		return result;
	}
}