#include "PacketPayloadValidatorTests.h"

#include <cstdint>
#include <string_view>

#include <Common/Game/InputFlags.h>
#include <Common/Packet/Game/GamePacket.h>

#include <Server/Net/PacketPayloadValidator.h>

#include <Tests/DebugTestResult.h>

namespace
{
	void RunValidateInputCommandSucceededTest(tests::DebugTestResult& result)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 1;
		packet.inputFlags = common::game::InputFlags::Up | common::game::InputFlags::Right;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateInputCommandPacket(packet);

		tests::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::Succeeded,
			"PacketPayloadValidator: valid input command succeeds");
	}

	void RunValidateInputCommandInvalidSequenceTest(tests::DebugTestResult& result)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 0;
		packet.inputFlags = common::game::InputFlags::Up;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateInputCommandPacket(packet);

		tests::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::InvalidInputSequence,
			"PacketPayloadValidator: input sequence zero rejected");
	}

	void RunValidateInputCommandInvalidFlagsTest(tests::DebugTestResult& result)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 1;
		packet.inputFlags = static_cast<common::game::InputFlags>(0x80);

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateInputCommandPacket(packet);

		tests::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::InvalidInputFlags,
			"PacketPayloadValidator: invalid input flags rejected");
	}

	void RunValidateInputCommandNoneFlagsAllowedTest(tests::DebugTestResult& result)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 1;
		packet.inputFlags = common::game::InputFlags::None;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateInputCommandPacket(packet);

		tests::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::Succeeded,
			"PacketPayloadValidator: none input flags allowed");
	}

	void RunValidateJoinRoomRequestSucceededTest(tests::DebugTestResult& result)
	{
		common::packet::JoinRoomRequestPacket packet{};
		packet.roomId = 1;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateJoinRoomRequestPacket(packet);

		tests::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::Succeeded,
			"PacketPayloadValidator: valid room id succeeds");
	}

	void RunValidateJoinRoomRequestZeroRoomRejectedTest(tests::DebugTestResult& result)
	{
		common::packet::JoinRoomRequestPacket packet{};
		packet.roomId = 0;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateJoinRoomRequestPacket(packet);

		tests::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::InvalidRoomId,
			"PacketPayloadValidator: zero room id rejected");
	}

	void RunValidateJoinRoomRequestNegativeRoomRejectedTest(tests::DebugTestResult& result)
	{
		common::packet::JoinRoomRequestPacket packet{};
		packet.roomId = -1;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateJoinRoomRequestPacket(packet);

		tests::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::InvalidRoomId,
			"PacketPayloadValidator: negative room id rejected");
	}

	void RunToStringTest(tests::DebugTestResult& result)
	{
		using Status = server::net::PacketPayloadValidator::PayloadValidationStatus;

		tests::Expect(result, std::string_view(server::net::PacketPayloadValidator::ToString(Status::Succeeded)) == "Succeeded",
			"PacketPayloadValidator: ToString Succeeded");
		tests::Expect(result,
			std::string_view(server::net::PacketPayloadValidator::ToString(Status::InvalidInputSequence)) == "InvalidInputSequence",
			"PacketPayloadValidator: ToString InvalidInputSequence");
		tests::Expect(result,
			std::string_view(server::net::PacketPayloadValidator::ToString(Status::InvalidInputFlags)) == "InvalidInputFlags",
			"PacketPayloadValidator: ToString InvalidInputFlags");
		tests::Expect(result,
			std::string_view(server::net::PacketPayloadValidator::ToString(Status::InvalidRoomId)) == "InvalidRoomId",
			"PacketPayloadValidator: ToString InvalidRoomId");

		tests::Expect(
			result,
			std::string_view(
				server::net::PacketPayloadValidator::ToString(
					static_cast<server::net::PacketPayloadValidator::PayloadValidationStatus>(999)
				)
			) == "Unknown",
			"PacketPayloadValidator: ToString Unknown"
		);
	}
}

namespace tests::server
{
	tests::DebugTestResult RunPacketPayloadValidatorTests()
	{
		tests::DebugTestResult result{};

		RunValidateInputCommandSucceededTest(result);
		RunValidateInputCommandInvalidSequenceTest(result);
		RunValidateInputCommandInvalidFlagsTest(result);
		RunValidateInputCommandNoneFlagsAllowedTest(result);
		RunValidateJoinRoomRequestSucceededTest(result);
		RunValidateJoinRoomRequestZeroRoomRejectedTest(result);
		RunValidateJoinRoomRequestNegativeRoomRejectedTest(result);
		RunToStringTest(result);

		return result;
	}
}