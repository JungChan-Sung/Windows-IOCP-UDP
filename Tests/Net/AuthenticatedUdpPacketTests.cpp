#include "AuthenticatedUdpPacketTests.h"

#include <optional>

#include <Common/Game/InputFlags.h>
#include <Common/Net/Auth/AuthenticatedUdpPacket.h>
#include <Common/Net/Reliable/ReliableUdpPacketBuilder.h>
#include <Common/Net/Reliable/ReliableUdpPacketHeader.h>
#include <Common/Net/SessionToken.h>
#include <Common/Packet/Game/CommandPacket.h>
#include <Common/Packet/Game/SessionPacket.h>
#include <Common/Packet/PacketConstants.h>
#include <Common/Packet/PacketSerialization.h>

namespace
{
	[[nodiscard]] constexpr common::net::SessionToken MakeSessionToken() noexcept
	{
		return common::net::SessionToken{
			.high = 1,
			.low = 2,
		};
	}

	void RunUnreliableRoundTripTest(tests::DebugTestResult& result)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 10;
		packet.inputFlags = common::game::InputFlags::Up;

		const std::optional<common::packet::PacketBuffer> serializedPacket =
			common::packet::SerializePacket(packet);

		tests::Expect(
			result,
			serializedPacket.has_value(),
			"AuthenticatedUdpPacket: unreliable base serialize"
		);

		if (!serializedPacket.has_value())
		{
			return;
		}

		const std::optional<common::packet::PacketBuffer> authenticatedPacket =
			common::net::BuildAuthenticatedUdpPacket(
				MakeSessionToken(),
				100,
				common::packet::ConstPacketSpan(
					serializedPacket->data(),
					serializedPacket->size()
				)
			);

		tests::Expect(
			result,
			authenticatedPacket.has_value(),
			"AuthenticatedUdpPacket: unreliable authenticated"
		);

		if (!authenticatedPacket.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			authenticatedPacket->size()
			== serializedPacket->size()
			+ common::net::authenticatedUdpPacketTrailerWireSize,
			"AuthenticatedUdpPacket: unreliable trailer size"
		);

		const std::optional<common::net::AuthenticatedUdpPacketView> packetView =
			common::net::ParseAuthenticatedUdpPacket(
				authenticatedPacket->data(),
				static_cast<int>(authenticatedPacket->size())
			);

		tests::Expect(
			result,
			packetView.has_value(),
			"AuthenticatedUdpPacket: unreliable parse"
		);

		if (!packetView.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			packetView->authentication.sequence == 100,
			"AuthenticatedUdpPacket: authentication sequence"
		);

		tests::Expect(
			result,
			!common::packet::IsReliablePacketHeader(
				packetView->packetHeader
			),
			"AuthenticatedUdpPacket: unreliable flag preserved"
		);

		tests::Expect(
			result,
			common::net::VerifyAuthenticatedUdpPacket(
				MakeSessionToken(),
				*packetView
			),
			"AuthenticatedUdpPacket: unreliable tag verifies"
		);

		const std::optional<common::packet::PacketBuffer> restoredPacket =
			common::net::BuildUnauthenticatedUdpPacket(
				*packetView
			);

		tests::Expect(
			result,
			restoredPacket.has_value(),
			"AuthenticatedUdpPacket: unreliable unwrap"
		);

		if (restoredPacket.has_value())
		{
			tests::Expect(
				result,
				*restoredPacket == *serializedPacket,
				"AuthenticatedUdpPacket: unreliable roundtrip"
			);
		}
	}

	void RunReliableRoundTripTest(tests::DebugTestResult& result)
	{
		const common::packet::LeaveRequestPacket packet{};

		const std::optional<common::packet::PacketBuffer> gamePacket =
			common::packet::SerializePacket(packet);

		tests::Expect(
			result,
			gamePacket.has_value(),
			"AuthenticatedUdpPacket: reliable game serialize"
		);

		if (!gamePacket.has_value())
		{
			return;
		}

		common::net::ReliableUdpPacketHeader reliableHeader{};
		reliableHeader.sequence = 7;

		const std::optional<common::packet::PacketBuffer> reliablePacket =
			common::net::BuildReliableUdpPacket(
				reliableHeader,
				common::packet::ConstPacketSpan(
					gamePacket->data(),
					gamePacket->size()
				)
			);

		tests::Expect(
			result,
			reliablePacket.has_value(),
			"AuthenticatedUdpPacket: reliable wrapper"
		);

		if (!reliablePacket.has_value())
		{
			return;
		}

		const std::optional<common::packet::PacketBuffer> authenticatedPacket =
			common::net::BuildAuthenticatedUdpPacket(
				MakeSessionToken(),
				200,
				common::packet::ConstPacketSpan(
					reliablePacket->data(),
					reliablePacket->size()
				)
			);

		tests::Expect(
			result,
			authenticatedPacket.has_value(),
			"AuthenticatedUdpPacket: reliable authenticated"
		);

		if (!authenticatedPacket.has_value())
		{
			return;
		}

		const std::optional<common::net::AuthenticatedUdpPacketView> packetView =
			common::net::ParseAuthenticatedUdpPacket(
				authenticatedPacket->data(),
				static_cast<int>(authenticatedPacket->size())
			);

		tests::Expect(
			result,
			packetView.has_value(),
			"AuthenticatedUdpPacket: reliable parse"
		);

		if (!packetView.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			common::packet::IsReliablePacketHeader(
				packetView->packetHeader
			),
			"AuthenticatedUdpPacket: reliable flag preserved"
		);

		tests::Expect(
			result,
			common::net::VerifyAuthenticatedUdpPacket(
				MakeSessionToken(),
				*packetView
			),
			"AuthenticatedUdpPacket: reliable tag verifies"
		);

		const std::optional<common::packet::PacketBuffer> restoredPacket =
			common::net::BuildUnauthenticatedUdpPacket(
				*packetView
			);

		tests::Expect(
			result,
			restoredPacket.has_value(),
			"AuthenticatedUdpPacket: reliable unwrap"
		);

		if (restoredPacket.has_value())
		{
			tests::Expect(
				result,
				*restoredPacket == *reliablePacket,
				"AuthenticatedUdpPacket: reliable roundtrip"
			);
		}
	}

	void RunTamperedPayloadRejectedTest(tests::DebugTestResult& result)
	{
		common::packet::InputCommandPacket packet{};
		packet.inputSequence = 10;

		const std::optional<common::packet::PacketBuffer> serializedPacket =
			common::packet::SerializePacket(packet);

		if (!serializedPacket.has_value())
		{
			tests::Expect(
				result,
				false,
				"AuthenticatedUdpPacket: tamper base serialize"
			);
			return;
		}

		std::optional<common::packet::PacketBuffer> authenticatedPacket =
			common::net::BuildAuthenticatedUdpPacket(
				MakeSessionToken(),
				100,
				common::packet::ConstPacketSpan(
					serializedPacket->data(),
					serializedPacket->size()
				)
			);

		if (!authenticatedPacket.has_value())
		{
			tests::Expect(
				result,
				false,
				"AuthenticatedUdpPacket: tamper authenticated build"
			);
			return;
		}

		(*authenticatedPacket)[common::packet::serializedPacketHeaderSize] ^= 0x01;

		const std::optional<common::net::AuthenticatedUdpPacketView> packetView =
			common::net::ParseAuthenticatedUdpPacket(
				authenticatedPacket->data(),
				static_cast<int>(authenticatedPacket->size())
			);

		tests::Expect(
			result,
			packetView.has_value(),
			"AuthenticatedUdpPacket: tampered packet still structurally parses"
		);

		if (packetView.has_value())
		{
			tests::Expect(
				result,
				!common::net::VerifyAuthenticatedUdpPacket(
					MakeSessionToken(),
					*packetView
				),
				"AuthenticatedUdpPacket: tampered payload rejected"
			);
		}
	}

	void RunWrongSessionTokenRejectedTest(tests::DebugTestResult& result)
	{
		const common::packet::FireRequestPacket packet{};

		const std::optional<common::packet::PacketBuffer> serializedPacket =
			common::packet::SerializePacket(packet);

		if (!serializedPacket.has_value())
		{
			tests::Expect(
				result,
				false,
				"AuthenticatedUdpPacket: wrong token base serialize"
			);
			return;
		}

		const std::optional<common::packet::PacketBuffer> authenticatedPacket =
			common::net::BuildAuthenticatedUdpPacket(
				MakeSessionToken(),
				100,
				common::packet::ConstPacketSpan(
					serializedPacket->data(),
					serializedPacket->size()
				)
			);

		if (!authenticatedPacket.has_value())
		{
			tests::Expect(
				result,
				false,
				"AuthenticatedUdpPacket: wrong token authenticated build"
			);
			return;
		}

		const std::optional<common::net::AuthenticatedUdpPacketView> packetView =
			common::net::ParseAuthenticatedUdpPacket(
				authenticatedPacket->data(),
				static_cast<int>(authenticatedPacket->size())
			);

		if (!packetView.has_value())
		{
			tests::Expect(
				result,
				false,
				"AuthenticatedUdpPacket: wrong token parse"
			);
			return;
		}

		const common::net::SessionToken wrongSessionToken{
			.high = 3,
			.low = 4,
		};

		tests::Expect(
			result,
			!common::net::VerifyAuthenticatedUdpPacket(
				wrongSessionToken,
				*packetView
			),
			"AuthenticatedUdpPacket: wrong session token rejected"
		);
	}

	void RunTamperedAuthenticationSequenceRejectedTest(tests::DebugTestResult& result)
	{
		const common::packet::FireRequestPacket packet{};

		const std::optional<common::packet::PacketBuffer> serializedPacket =
			common::packet::SerializePacket(packet);

		if (!serializedPacket.has_value())
		{
			tests::Expect(
				result,
				false,
				"AuthenticatedUdpPacket: sequence tamper base serialize"
			);
			return;
		}

		std::optional<common::packet::PacketBuffer> authenticatedPacket =
			common::net::BuildAuthenticatedUdpPacket(
				MakeSessionToken(),
				100,
				common::packet::ConstPacketSpan(
					serializedPacket->data(),
					serializedPacket->size()
				)
			);

		if (!authenticatedPacket.has_value())
		{
			tests::Expect(
				result,
				false,
				"AuthenticatedUdpPacket: sequence tamper authenticated build"
			);
			return;
		}

		const std::size_t sequenceOffset =
			authenticatedPacket->size()
			- common::net::packetAuthenticationTagSize
			- common::net::packetAuthenticationSequenceWireSize;

		(*authenticatedPacket)[sequenceOffset] ^= 0x01;

		const std::optional<common::net::AuthenticatedUdpPacketView> packetView =
			common::net::ParseAuthenticatedUdpPacket(
				authenticatedPacket->data(),
				static_cast<int>(authenticatedPacket->size())
			);

		tests::Expect(
			result,
			packetView.has_value(),
			"AuthenticatedUdpPacket: sequence tamper structurally parses"
		);

		if (!packetView.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			!common::net::VerifyAuthenticatedUdpPacket(
				MakeSessionToken(),
				*packetView
			),
			"AuthenticatedUdpPacket: tampered authentication sequence rejected"
		);
	}

	void RunTamperedReliableHeaderRejectedTest(tests::DebugTestResult& result)
	{
		const common::packet::LeaveRequestPacket packet{};

		const std::optional<common::packet::PacketBuffer> serializedPacket =
			common::packet::SerializePacket(packet);

		tests::Expect(
			result,
			serializedPacket.has_value(),
			"AuthenticatedUdpPacket: reliable header tamper base serialize"
		);

		if (!serializedPacket.has_value())
		{
			return;
		}

		common::net::ReliableUdpPacketHeader reliableHeader{};
		reliableHeader.sequence = 10;
		reliableHeader.ackSequence = 20;
		reliableHeader.ackBitfield = 0x00000005;

		const std::optional<common::packet::PacketBuffer> reliablePacket =
			common::net::BuildReliableUdpPacket(
				reliableHeader,
				common::packet::ConstPacketSpan(
					serializedPacket->data(),
					serializedPacket->size()
				)
			);

		tests::Expect(
			result,
			reliablePacket.has_value(),
			"AuthenticatedUdpPacket: reliable header tamper wrapper"
		);

		if (!reliablePacket.has_value())
		{
			return;
		}

		std::optional<common::packet::PacketBuffer> authenticatedPacket =
			common::net::BuildAuthenticatedUdpPacket(
				MakeSessionToken(),
				200,
				common::packet::ConstPacketSpan(
					reliablePacket->data(),
					reliablePacket->size()
				)
			);

		tests::Expect(
			result,
			authenticatedPacket.has_value(),
			"AuthenticatedUdpPacket: reliable header tamper authenticated"
		);

		if (!authenticatedPacket.has_value())
		{
			return;
		}

		const std::size_t ackSequenceOffset =
			common::net::reliableUdpPacketHeaderOffset
			+ common::packet::uint32WireSize;

		(*authenticatedPacket)[ackSequenceOffset] ^= 0x01;

		const std::optional<common::net::AuthenticatedUdpPacketView> packetView =
			common::net::ParseAuthenticatedUdpPacket(
				authenticatedPacket->data(),
				static_cast<int>(authenticatedPacket->size())
			);

		tests::Expect(
			result,
			packetView.has_value(),
			"AuthenticatedUdpPacket: reliable header tamper structurally parses"
		);

		if (!packetView.has_value())
		{
			return;
		}

		tests::Expect(
			result,
			!common::net::VerifyAuthenticatedUdpPacket(
				MakeSessionToken(),
				*packetView
			),
			"AuthenticatedUdpPacket: tampered reliable ack sequence rejected"
		);
	}
}

namespace tests::net
{
	DebugTestResult RunAuthenticatedUdpPacketTests()
	{
		DebugTestResult result{};

		RunUnreliableRoundTripTest(result);
		RunReliableRoundTripTest(result);

		RunTamperedPayloadRejectedTest(result);
		RunTamperedReliableHeaderRejectedTest(result);
		RunTamperedAuthenticationSequenceRejectedTest(result);
		RunWrongSessionTokenRejectedTest(result);

		return result;
	}
}