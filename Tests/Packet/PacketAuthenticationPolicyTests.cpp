#include "PacketAuthenticationPolicyTests.h"

#include <Common/Packet/PacketAuthenticationPolicy.h>
#include <Common/Packet/PacketType.h>

namespace
{
	void RunBootstrapPacketTests(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			!common::packet::RequiresClientPacketAuthentication(
				common::packet::PacketType::AccountLoginRequest
			),
			"PacketAuthenticationPolicy: AccountLoginRequest does not require authentication"
		);

		tests::Expect(
			result,
			!common::packet::RequiresClientPacketAuthentication(
				common::packet::PacketType::JoinRequest
			),
			"PacketAuthenticationPolicy: JoinRequest does not require authentication"
		);
	}

	void RunAuthenticatedPacketTests(tests::DebugTestResult& result)
	{
		tests::Expect(
			result,
			common::packet::RequiresClientPacketAuthentication(
				common::packet::PacketType::None
			),
			"PacketAuthenticationPolicy: reliable ack-only packet requires authentication"
		);

		tests::Expect(
			result,
			common::packet::RequiresClientPacketAuthentication(
				common::packet::PacketType::InputCommand
			),
			"PacketAuthenticationPolicy: InputCommand requires authentication"
		);

		tests::Expect(
			result,
			common::packet::RequiresClientPacketAuthentication(
				common::packet::PacketType::FireRequest
			),
			"PacketAuthenticationPolicy: FireRequest requires authentication"
		);

		tests::Expect(
			result,
			common::packet::RequiresClientPacketAuthentication(
				common::packet::PacketType::KeepAlive
			),
			"PacketAuthenticationPolicy: KeepAlive requires authentication"
		);

		tests::Expect(
			result,
			common::packet::RequiresClientPacketAuthentication(
				common::packet::PacketType::LeaveRequest
			),
			"PacketAuthenticationPolicy: LeaveRequest requires authentication"
		);

		tests::Expect(
			result,
			common::packet::RequiresClientPacketAuthentication(
				common::packet::PacketType::JoinRoomRequest
			),
			"PacketAuthenticationPolicy: JoinRoomRequest requires authentication"
		);
	}
}

namespace tests::packet
{
	DebugTestResult RunPacketAuthenticationPolicyTests()
	{
		DebugTestResult result{};

		RunBootstrapPacketTests(result);
		RunAuthenticatedPacketTests(result);

		return result;
	}
}