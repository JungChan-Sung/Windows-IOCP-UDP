#pragma once

#include <array>
#include <cstdint>

#include <Common/Packet/Game/GamePacketConstants.h>
#include <Common/Packet/PacketHeader.h>

namespace common::packet
{
	struct PlayerStateData
	{
	public:
		std::uint32_t playerId = 0;
		float x = 0.0F;
		float y = 0.0F;

		std::int32_t hp = 0;
		std::uint8_t isDead = 0;

		float respawnRemainingSeconds = 0.0F;
		float invincibilityRemainingSeconds = 0.0F;
		float hitFlashRemainingSeconds = 0.0F;

		std::uint32_t killCount = 0;
		std::uint32_t deathCount = 0;
	};

	struct BulletStateData
	{
	public:
		std::uint32_t bulletId = 0;
		float x = 0.0F;
		float y = 0.0F;
	};

	struct PlayerSnapshotPacket
	{
	public:
		PacketHeader header{ 0, PacketType::PlayerSnapshot };
		std::uint32_t serverTick = 0;
		std::int32_t roomId = 0;
		std::uint32_t lastProcessedInputSequence = 0;

		std::uint16_t playerCount = 0;
		std::array<PlayerStateData, maxPlayersPerSnapshot> players{};
	};

	struct BulletSnapshotPacket
	{
	public:
		PacketHeader header{ 0, PacketType::BulletSnapshot };
		std::uint32_t serverTick = 0;
		std::int32_t roomId = 0;

		std::uint16_t chunkIndex = 0;
		std::uint16_t chunkCount = 1;

		std::uint16_t bulletCount = 0;
		std::array<BulletStateData, maxBulletsPerSnapshot> bullets{};
	};
}