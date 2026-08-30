#pragma once

#include <cstdint>
#include <vector>

#include <Common/Game/EffectType.h>
#include <Common/Game/GameTypes.h>
#include <Common/Net/EndpointKey.h>

namespace server::protocol
{
	struct SnapshotPeerContext
	{
	public:
		common::net::EndpointKey endpointKey{};
		common::game::PlayerId playerId = 0;
		std::uint32_t lastProcessedInputSequence = 0;
	};

	using SnapshotPeerContextList = std::vector<SnapshotPeerContext>;

	struct SnapshotPlayerStateContext
	{
	public:
		common::game::PlayerId playerId = 0;

		float x = 0.0F;
		float y = 0.0F;

		int hp = 0;
		bool isDead = false;

		std::uint32_t killCount = 0;
		std::uint32_t deathCount = 0;

		float respawnRemainingSeconds = 0.0F;
		float invincibilityRemainingSeconds = 0.0F;
		float hitFlashRemainingSeconds = 0.0F;
	};

	using SnapshotPlayerStateContextList = std::vector<SnapshotPlayerStateContext>;

	struct SnapshotBulletStateContext
	{
	public:
		common::game::BulletId bulletId = 0;
		float x = 0.0F;
		float y = 0.0F;
	};

	using SnapshotBulletStateContextList = std::vector<SnapshotBulletStateContext>;

	struct SnapshotImpactEffectContext
	{
	public:
		common::game::EffectType effectType = common::game::EffectType::None;
		float x = 0.0F;
		float y = 0.0F;
	};

	using SnapshotImpactEffectContextList = std::vector<SnapshotImpactEffectContext>;

	struct SnapshotRoomContext
	{
	public:
		common::game::RoomId roomId = 0;

		SnapshotPeerContextList peerContextList;
		SnapshotPlayerStateContextList playerStateContextList;
		SnapshotBulletStateContextList bulletStateContextList;
		SnapshotImpactEffectContextList impactEffectContextList;
	};

	using SnapshotRoomContextList = std::vector<SnapshotRoomContext>;

	struct SnapshotBroadcastContext
	{
	public:
		std::uint32_t serverTick = 0;
		std::uint32_t serverTickIntervalMilliseconds = 0;

		SnapshotRoomContextList roomContextList;
	};
}