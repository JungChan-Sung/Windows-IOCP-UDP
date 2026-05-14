#include "PacketPayloadValidatorTests.h"

#include <cstdint>
#include <string_view>

#include <Common/Diagnostics/DebugTestResult.h>
#include <Common/Game/InputFlags.h>
#include <Common/Packet/GamePacket.h>

#include <Server/Net/PacketPayloadValidator.h>

namespace
{
	void RunValidateInputCommandSucceededTest(common::diagnostics::DebugTestResult& result)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 1;
		packet.inputFlags = common::game::InputFlags::Up | common::game::InputFlags::Right;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateInputCommandPacket(packet);

		common::diagnostics::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::Succeeded,
			"PacketPayloadValidator: valid input command succeeds");
	}

	void RunValidateInputCommandInvalidSequenceTest(common::diagnostics::DebugTestResult& result)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 0;
		packet.inputFlags = common::game::InputFlags::Up;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateInputCommandPacket(packet);

		common::diagnostics::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::InvalidInputSequence,
			"PacketPayloadValidator: input sequence zero rejected");
	}

	void RunValidateInputCommandInvalidFlagsTest(common::diagnostics::DebugTestResult& result)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 1;
		packet.inputFlags = static_cast<common::game::InputFlags>(0x80);

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateInputCommandPacket(packet);

		common::diagnostics::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::InvalidInputFlags,
			"PacketPayloadValidator: invalid input flags rejected");
	}

	void RunValidateInputCommandNoneFlagsAllowedTest(common::diagnostics::DebugTestResult& result)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 1;
		packet.inputFlags = common::game::InputFlags::None;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateInputCommandPacket(packet);

		common::diagnostics::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::Succeeded,
			"PacketPayloadValidator: none input flags allowed");
	}

	void RunValidateJoinRoomRequestSucceededTest(common::diagnostics::DebugTestResult& result)
	{
		common::packet::JoinRoomRequestPacket packet{};
		packet.roomId = 1;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateJoinRoomRequestPacket(packet);

		common::diagnostics::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::Succeeded,
			"PacketPayloadValidator: valid room id succeeds");
	}

	void RunValidateJoinRoomRequestZeroRoomRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		common::packet::JoinRoomRequestPacket packet{};
		packet.roomId = 0;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateJoinRoomRequestPacket(packet);

		common::diagnostics::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::InvalidRoomId,
			"PacketPayloadValidator: zero room id rejected");
	}

	void RunValidateJoinRoomRequestNegativeRoomRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		common::packet::JoinRoomRequestPacket packet{};
		packet.roomId = -1;

		const server::net::PacketPayloadValidator::PayloadValidationStatus status
			= server::net::PacketPayloadValidator::ValidateJoinRoomRequestPacket(packet);

		common::diagnostics::Expect(result, status == server::net::PacketPayloadValidator::PayloadValidationStatus::InvalidRoomId,
			"PacketPayloadValidator: negative room id rejected");
	}

	void RunToStringTest(common::diagnostics::DebugTestResult& result)
	{
		using Status = server::net::PacketPayloadValidator::PayloadValidationStatus;

		common::diagnostics::Expect(result, std::string_view(server::net::PacketPayloadValidator::ToString(Status::Succeeded)) == "Succeeded",
			"PacketPayloadValidator: ToString Succeeded");
		common::diagnostics::Expect(result,
			std::string_view(server::net::PacketPayloadValidator::ToString(Status::InvalidInputSequence)) == "InvalidInputSequence",
			"PacketPayloadValidator: ToString InvalidInputSequence");
		common::diagnostics::Expect(result,
			std::string_view(server::net::PacketPayloadValidator::ToString(Status::InvalidInputFlags)) == "InvalidInputFlags",
			"PacketPayloadValidator: ToString InvalidInputFlags");
		common::diagnostics::Expect(result,
			std::string_view(server::net::PacketPayloadValidator::ToString(Status::InvalidRoomId)) == "InvalidRoomId",
			"PacketPayloadValidator: ToString InvalidRoomId");
	}
}

namespace tests::server
{
	common::diagnostics::DebugTestResult RunPacketPayloadValidatorTests()
	{
		common::diagnostics::DebugTestResult result{};

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