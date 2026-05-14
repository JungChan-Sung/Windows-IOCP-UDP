#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

#include <Common/Game/InputFlags.h>

#include "PacketConstants.h"
#include "PacketHeader.h"

namespace common::packet
{
	enum class EffectType : std::uint8_t
	{
		None = 0,
		Impact = 1,
		Spawn = 2,
	};

	struct JoinRequestPacket
	{
	public:
		PacketHeader header{ 0, PacketType::JoinRequest };
	};

	struct JoinResponsePacket
	{
	public:
		PacketHeader header{ 0, PacketType::JoinResponse };
		std::uint32_t playerId = 0;
		float spawnX = 0.0F;
		float spawnY = 0.0F;
	};

	struct InputCommandPacket
	{
	public:
		PacketHeader header{ 0, PacketType::InputCommand };
		std::uint32_t inputSequence = 0;
		game::InputFlags inputFlags = game::InputFlags::None;
	};

	struct FireRequestPacket
	{
	public:
		PacketHeader header{ 0, PacketType::FireRequest };
	};

	struct LeaveRequestPacket
	{
	public:
		PacketHeader header{ 0, PacketType::LeaveRequest };
	};

	struct JoinRoomRequestPacket
	{
	public:
		PacketHeader header{ 0, PacketType::JoinRoomRequest };
		std::int32_t roomId = 0;
	};

	struct JoinRoomResponsePacket
	{
	public:
		PacketHeader header{ 0, PacketType::JoinRoomResponse };
		std::int32_t roomId = 0;
		float spawnX = 0.0F;
		float spawnY = 0.0F;
	};

	struct PlayerJoinedPacket
	{
	public:
		PacketHeader header{ 0, PacketType::PlayerJoined };
		std::uint32_t playerId = 0;
		std::int32_t roomId = 0;
		float x = 0.0F;
		float y = 0.0F;
	};

	struct PlayerLeftPacket
	{
	public:
		PacketHeader header{ 0, PacketType::PlayerLeft };
		std::uint32_t playerId = 0;
		std::int32_t roomId = 0;
	};

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

	struct ImpactEffectData
	{
	public:
		EffectType effectType = EffectType::None;
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

	struct ImpactEffectPacket
	{
	public:
		PacketHeader header{ 0, PacketType::ImpactEffect };
		std::uint32_t serverTick = 0;
		std::int32_t roomId = 0;

		std::uint16_t chunkIndex = 0;
		std::uint16_t chunkCount = 1;

		std::uint16_t effectCount = 0;
		std::array<ImpactEffectData, maxImpactEffectsPerPacket> effects{};
	};
}