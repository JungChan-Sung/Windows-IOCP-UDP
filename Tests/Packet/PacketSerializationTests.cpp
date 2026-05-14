#include "PacketSerializationTests.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <Common/Diagnostics/DebugTestResult.h>
#include <Common/Packet/PacketSerialization.h>

namespace
{
	void WriteUInt16ToBuffer(common::packet::PacketBuffer& buffer, std::size_t offset, std::uint16_t value)
	{
		if (offset + 1 >= buffer.size())
		{
			return;
		}

		buffer[offset] = static_cast<char>(value & 0x00FF);
		buffer[offset + 1] = static_cast<char>((value >> 8) & 0x00FF);
	}

	template <typename TPacket>
	[[nodiscard]] std::optional<TPacket> RoundTrip(
		common::diagnostics::DebugTestResult& result,
		const TPacket& packet,
		std::string_view testName
	)
	{
		const std::optional<common::packet::PacketBuffer> serializedPacket = common::packet::SerializePacket(packet);
		common::diagnostics::Expect(result, serializedPacket.has_value(), std::string(testName) + ": serialize");

		if (!serializedPacket.has_value())
		{
			return std::nullopt;
		}

		const std::size_t expectedSize = common::packet::PacketCodec<TPacket>::GetSerializedSize(packet);
		common::diagnostics::Expect(result, serializedPacket->size() == expectedSize, std::string(testName) + ": serialized size");

		const std::optional<TPacket> deserializedPacket = common::packet::DeserializePacket<TPacket>(
			serializedPacket->data(),
			static_cast<int>(serializedPacket->size())
		);

		common::diagnostics::Expect(result, deserializedPacket.has_value(), std::string(testName) + ": deserialize");

		if (!deserializedPacket.has_value())
		{
			return std::nullopt;
		}

		common::diagnostics::Expect(result, deserializedPacket->header.size == expectedSize, std::string(testName) + ": header size");
		common::diagnostics::Expect(result, deserializedPacket->header.type == common::packet::PacketCodec<TPacket>::packetType,
			std::string(testName) + ": header type");
		common::diagnostics::Expect(result, deserializedPacket->header.version == common::packet::protocolVersion,
			std::string(testName) + ": header version");

		return deserializedPacket;
	}

	void RunFixedPacketRoundTripTests(common::diagnostics::DebugTestResult& result)
	{
		{
			common::packet::JoinRequestPacket packet{};
			const std::optional<common::packet::JoinRequestPacket> roundTripPacket = RoundTrip(result, packet, "JoinRequest");
			common::diagnostics::Expect(result, roundTripPacket.has_value(), "JoinRequest: roundtrip");
		}

		{
			common::packet::JoinResponsePacket packet{};
			packet.playerId = 1001;
			packet.spawnX = 120.5F;
			packet.spawnY = 240.25F;

			const std::optional<common::packet::JoinResponsePacket> roundTripPacket = RoundTrip(result, packet, "JoinResponse");
			if (roundTripPacket.has_value())
			{
				common::diagnostics::Expect(result, roundTripPacket->playerId == packet.playerId, "JoinResponse: playerId");
				common::diagnostics::Expect(result, roundTripPacket->spawnX == packet.spawnX, "JoinResponse: spawnX");
				common::diagnostics::Expect(result, roundTripPacket->spawnY == packet.spawnY, "JoinResponse: spawnY");
			}
		}

		{
			common::packet::InputCommandPacket packet{};
			packet.inputSequence = 77;
			packet.inputFlags = common::game::InputFlags::Up | common::game::InputFlags::Left;

			const std::optional<common::packet::InputCommandPacket> roundTripPacket = RoundTrip(result, packet, "InputCommand");
			if (roundTripPacket.has_value())
			{
				common::diagnostics::Expect(result, roundTripPacket->inputSequence == packet.inputSequence, "InputCommand: inputSequence");
				common::diagnostics::Expect(result, roundTripPacket->inputFlags == packet.inputFlags, "InputCommand: inputFlags");
			}
		}

		{
			common::packet::FireRequestPacket packet{};
			const std::optional<common::packet::FireRequestPacket> roundTripPacket = RoundTrip(result, packet, "FireRequest");
			common::diagnostics::Expect(result, roundTripPacket.has_value(), "FireRequest: roundtrip");
		}

		{
			common::packet::LeaveRequestPacket packet{};
			const std::optional<common::packet::LeaveRequestPacket> roundTripPacket = RoundTrip(result, packet, "LeaveRequest");
			common::diagnostics::Expect(result, roundTripPacket.has_value(), "LeaveRequest: roundtrip");
		}

		{
			common::packet::JoinRoomRequestPacket packet{};
			packet.roomId = 3;

			const std::optional<common::packet::JoinRoomRequestPacket> roundTripPacket = RoundTrip(result, packet, "JoinRoomRequest");
			if (roundTripPacket.has_value())
			{
				common::diagnostics::Expect(result, roundTripPacket->roomId == packet.roomId, "JoinRoomRequest: roomId");
			}
		}

		{
			common::packet::JoinRoomResponsePacket packet{};
			packet.roomId = 4;
			packet.spawnX = 300.0F;
			packet.spawnY = 400.0F;

			const std::optional<common::packet::JoinRoomResponsePacket> roundTripPacket = RoundTrip(result, packet, "JoinRoomResponse");
			if (roundTripPacket.has_value())
			{
				common::diagnostics::Expect(result, roundTripPacket->roomId == packet.roomId, "JoinRoomResponse: roomId");
				common::diagnostics::Expect(result, roundTripPacket->spawnX == packet.spawnX, "JoinRoomResponse: spawnX");
				common::diagnostics::Expect(result, roundTripPacket->spawnY == packet.spawnY, "JoinRoomResponse: spawnY");
			}
		}

		{
			common::packet::PlayerJoinedPacket packet{};
			packet.playerId = 5;
			packet.roomId = 2;
			packet.x = 10.0F;
			packet.y = 20.0F;

			const std::optional<common::packet::PlayerJoinedPacket> roundTripPacket = RoundTrip(result, packet, "PlayerJoined");
			if (roundTripPacket.has_value())
			{
				common::diagnostics::Expect(result, roundTripPacket->playerId == packet.playerId, "PlayerJoined: playerId");
				common::diagnostics::Expect(result, roundTripPacket->roomId == packet.roomId, "PlayerJoined: roomId");
				common::diagnostics::Expect(result, roundTripPacket->x == packet.x, "PlayerJoined: x");
				common::diagnostics::Expect(result, roundTripPacket->y == packet.y, "PlayerJoined: y");
			}
		}

		{
			common::packet::PlayerLeftPacket packet{};
			packet.playerId = 6;
			packet.roomId = 2;

			const std::optional<common::packet::PlayerLeftPacket> roundTripPacket = RoundTrip(result, packet, "PlayerLeft");
			if (roundTripPacket.has_value())
			{
				common::diagnostics::Expect(result, roundTripPacket->playerId == packet.playerId, "PlayerLeft: playerId");
				common::diagnostics::Expect(result, roundTripPacket->roomId == packet.roomId, "PlayerLeft: roomId");
			}
		}
	}

	void RunPlayerSnapshotRoundTripTest(common::diagnostics::DebugTestResult& result)
	{
		common::packet::PlayerSnapshotPacket packet{};
		packet.serverTick = 1234;
		packet.roomId = 1;
		packet.lastProcessedInputSequence = 88;
		packet.playerCount = 2;

		packet.players[0].playerId = 1;
		packet.players[0].x = 10.0F;
		packet.players[0].y = 20.0F;
		packet.players[0].hp = 3;
		packet.players[0].isDead = 0;
		packet.players[0].respawnRemainingSeconds = 0.0F;
		packet.players[0].invincibilityRemainingSeconds = 1.0F;
		packet.players[0].hitFlashRemainingSeconds = 0.0F;
		packet.players[0].killCount = 7;
		packet.players[0].deathCount = 1;

		packet.players[1].playerId = 2;
		packet.players[1].x = 30.0F;
		packet.players[1].y = 40.0F;
		packet.players[1].hp = 0;
		packet.players[1].isDead = 1;
		packet.players[1].respawnRemainingSeconds = 2.5F;
		packet.players[1].invincibilityRemainingSeconds = 0.0F;
		packet.players[1].hitFlashRemainingSeconds = 0.1F;
		packet.players[1].killCount = 3;
		packet.players[1].deathCount = 5;

		const std::optional<common::packet::PlayerSnapshotPacket> roundTripPacket = RoundTrip(result, packet, "PlayerSnapshot");
		if (!roundTripPacket.has_value())
		{
			return;
		}

		common::diagnostics::Expect(result, roundTripPacket->serverTick == packet.serverTick, "PlayerSnapshot: serverTick");
		common::diagnostics::Expect(result, roundTripPacket->roomId == packet.roomId, "PlayerSnapshot: roomId");
		common::diagnostics::Expect(result, roundTripPacket->lastProcessedInputSequence == packet.lastProcessedInputSequence,
			"PlayerSnapshot: lastProcessedInputSequence");
		common::diagnostics::Expect(result, roundTripPacket->playerCount == packet.playerCount, "PlayerSnapshot: playerCount");

		for (std::size_t index = 0; index < packet.playerCount; ++index)
		{
			common::diagnostics::Expect(result, roundTripPacket->players[index].playerId == packet.players[index].playerId,
				"PlayerSnapshot: playerId");
			common::diagnostics::Expect(result, roundTripPacket->players[index].x == packet.players[index].x, "PlayerSnapshot: x");
			common::diagnostics::Expect(result, roundTripPacket->players[index].y == packet.players[index].y, "PlayerSnapshot: y");
			common::diagnostics::Expect(result, roundTripPacket->players[index].hp == packet.players[index].hp, "PlayerSnapshot: hp");
			common::diagnostics::Expect(result, roundTripPacket->players[index].isDead == packet.players[index].isDead,
				"PlayerSnapshot: isDead");
		}
	}

	void RunBulletSnapshotRoundTripTest(common::diagnostics::DebugTestResult& result)
	{
		common::packet::BulletSnapshotPacket packet{};
		packet.serverTick = 2000;
		packet.roomId = 2;
		packet.chunkIndex = 1;
		packet.chunkCount = 3;
		packet.bulletCount = 2;

		packet.bullets[0].bulletId = 100;
		packet.bullets[0].x = 11.0F;
		packet.bullets[0].y = 22.0F;

		packet.bullets[1].bulletId = 101;
		packet.bullets[1].x = 33.0F;
		packet.bullets[1].y = 44.0F;

		const std::optional<common::packet::BulletSnapshotPacket> roundTripPacket = RoundTrip(result, packet, "BulletSnapshot");
		if (!roundTripPacket.has_value())
		{
			return;
		}

		common::diagnostics::Expect(result, roundTripPacket->serverTick == packet.serverTick, "BulletSnapshot: serverTick");
		common::diagnostics::Expect(result, roundTripPacket->roomId == packet.roomId, "BulletSnapshot: roomId");
		common::diagnostics::Expect(result, roundTripPacket->chunkIndex == packet.chunkIndex, "BulletSnapshot: chunkIndex");
		common::diagnostics::Expect(result, roundTripPacket->chunkCount == packet.chunkCount, "BulletSnapshot: chunkCount");
		common::diagnostics::Expect(result, roundTripPacket->bulletCount == packet.bulletCount, "BulletSnapshot: bulletCount");
		common::diagnostics::Expect(result, roundTripPacket->bullets[0].bulletId == packet.bullets[0].bulletId, "BulletSnapshot: bullet 0");
		common::diagnostics::Expect(result, roundTripPacket->bullets[1].bulletId == packet.bullets[1].bulletId, "BulletSnapshot: bullet 1");
	}

	void RunImpactEffectRoundTripTest(common::diagnostics::DebugTestResult& result)
	{
		common::packet::ImpactEffectPacket packet{};
		packet.serverTick = 3000;
		packet.roomId = 3;
		packet.chunkIndex = 0;
		packet.chunkCount = 1;
		packet.effectCount = 2;

		packet.effects[0].effectType = common::packet::EffectType::Impact;
		packet.effects[0].x = 55.0F;
		packet.effects[0].y = 66.0F;

		packet.effects[1].effectType = common::packet::EffectType::Spawn;
		packet.effects[1].x = 77.0F;
		packet.effects[1].y = 88.0F;

		const std::optional<common::packet::ImpactEffectPacket> roundTripPacket = RoundTrip(result, packet, "ImpactEffect");
		if (!roundTripPacket.has_value())
		{
			return;
		}

		common::diagnostics::Expect(result, roundTripPacket->serverTick == packet.serverTick, "ImpactEffect: serverTick");
		common::diagnostics::Expect(result, roundTripPacket->roomId == packet.roomId, "ImpactEffect: roomId");
		common::diagnostics::Expect(result, roundTripPacket->chunkIndex == packet.chunkIndex, "ImpactEffect: chunkIndex");
		common::diagnostics::Expect(result, roundTripPacket->chunkCount == packet.chunkCount, "ImpactEffect: chunkCount");
		common::diagnostics::Expect(result, roundTripPacket->effectCount == packet.effectCount, "ImpactEffect: effectCount");
		common::diagnostics::Expect(result, roundTripPacket->effects[0].effectType == packet.effects[0].effectType, "ImpactEffect: effect 0");
		common::diagnostics::Expect(result, roundTripPacket->effects[1].effectType == packet.effects[1].effectType, "ImpactEffect: effect 1");
	}

	void RunInvalidPacketTests(common::diagnostics::DebugTestResult& result)
	{
		common::packet::JoinResponsePacket packet{};
		packet.playerId = 10;
		packet.spawnX = 20.0F;
		packet.spawnY = 30.0F;

		const std::optional<common::packet::PacketBuffer> serializedPacket = common::packet::SerializePacket(packet);
		common::diagnostics::Expect(result, serializedPacket.has_value(), "InvalidPacket: base serialize");

		if (!serializedPacket.has_value())
		{
			return;
		}

		{
			common::packet::PacketBuffer buffer = *serializedPacket;
			buffer.pop_back();

			const std::optional<common::packet::JoinResponsePacket> deserializedPacket
				= common::packet::DeserializePacket<common::packet::JoinResponsePacket>(buffer.data(), static_cast<int>(buffer.size()));

			common::diagnostics::Expect(result, !deserializedPacket.has_value(), "InvalidPacket: truncated rejected");
		}

		{
			common::packet::PacketBuffer buffer = *serializedPacket;
			WriteUInt16ToBuffer(buffer, 0, static_cast<std::uint16_t>(buffer.size() + 1));

			const std::optional<common::packet::JoinResponsePacket> deserializedPacket
				= common::packet::DeserializePacket<common::packet::JoinResponsePacket>(buffer.data(), static_cast<int>(buffer.size()));

			common::diagnostics::Expect(result, !deserializedPacket.has_value(), "InvalidPacket: wrong size rejected");
		}

		{
			common::packet::PacketBuffer buffer = *serializedPacket;
			WriteUInt16ToBuffer(buffer, 2, static_cast<std::uint16_t>(common::packet::PacketType::FireRequest));

			const std::optional<common::packet::JoinResponsePacket> deserializedPacket
				= common::packet::DeserializePacket<common::packet::JoinResponsePacket>(buffer.data(), static_cast<int>(buffer.size()));

			common::diagnostics::Expect(result, !deserializedPacket.has_value(), "InvalidPacket: wrong type rejected");
		}

		{
			common::packet::PacketBuffer buffer = *serializedPacket;
			WriteUInt16ToBuffer(buffer, 4, common::packet::protocolVersion + 1);

			const std::optional<common::packet::JoinResponsePacket> deserializedPacket
				= common::packet::DeserializePacket<common::packet::JoinResponsePacket>(buffer.data(), static_cast<int>(buffer.size()));

			common::diagnostics::Expect(result, !deserializedPacket.has_value(), "InvalidPacket: wrong version rejected");
		}
	}

	void RunInvalidVariablePacketTests(common::diagnostics::DebugTestResult& result)
	{
		common::packet::PlayerSnapshotPacket packet{};
		packet.serverTick = 1;
		packet.roomId = 1;
		packet.lastProcessedInputSequence = 1;
		packet.playerCount = 1;
		packet.players[0].playerId = 10;
		packet.players[0].x = 1.0F;
		packet.players[0].y = 2.0F;

		const std::optional<common::packet::PacketBuffer> serializedPacket = common::packet::SerializePacket(packet);
		common::diagnostics::Expect(result, serializedPacket.has_value(), "InvalidVariablePacket: base serialize");

		if (!serializedPacket.has_value())
		{
			return;
		}

		constexpr std::size_t playerCountOffset = common::packet::serializedPacketHeaderSize
			+ common::packet::uint32WireSize
			+ common::packet::int32WireSize
			+ common::packet::uint32WireSize;

		{
			common::packet::PacketBuffer buffer = *serializedPacket;
			WriteUInt16ToBuffer(buffer, playerCountOffset, static_cast<std::uint16_t>(common::packet::maxPlayersPerSnapshot + 1));

			const std::optional<common::packet::PlayerSnapshotPacket> deserializedPacket
				= common::packet::DeserializePacket<common::packet::PlayerSnapshotPacket>(buffer.data(), static_cast<int>(buffer.size()));

			common::diagnostics::Expect(result, !deserializedPacket.has_value(), "InvalidVariablePacket: count over max rejected");
		}

		{
			common::packet::PacketBuffer buffer = *serializedPacket;
			WriteUInt16ToBuffer(buffer, playerCountOffset, 0);

			const std::optional<common::packet::PlayerSnapshotPacket> deserializedPacket
				= common::packet::DeserializePacket<common::packet::PlayerSnapshotPacket>(buffer.data(), static_cast<int>(buffer.size()));

			common::diagnostics::Expect(result, !deserializedPacket.has_value(), "InvalidVariablePacket: count size mismatch rejected");
		}
	}
}

namespace tests::packet
{
	common::diagnostics::DebugTestResult RunPacketSerializationTests()
	{
		common::diagnostics::DebugTestResult result{};

		RunFixedPacketRoundTripTests(result);
		RunPlayerSnapshotRoundTripTest(result);
		RunBulletSnapshotRoundTripTest(result);
		RunImpactEffectRoundTripTest(result);
		RunInvalidPacketTests(result);
		RunInvalidVariablePacketTests(result);

		return result;
	}
}
