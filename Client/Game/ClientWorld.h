#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <Common/Packet/Game/GamePacket.h>
#include <Common/Game/GameTypes.h>
#include <Common/Game/InputFlags.h>
#include <Common/Time/TimeTypes.h>

#include <Client/Game/LocalPlayerPrediction.h>
#include <Client/Game/LocalPlayerReconciliation.h>

namespace client::game
{
	class ClientWorld
	{
	public:
		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;

	public:
		struct SnapshotSample
		{
		public:
			float x = 0.0F;
			float y = 0.0F;
			common::time::TimePoint time{};
		};

		struct RemotePlayerState
		{
		public:
			PlayerId playerId = 0;
			SnapshotSample previousSample{};
			SnapshotSample targetSample{};

			int hp = 0;
			bool isDead = false;

			float respawnRemainingSeconds = 0.0F;
			float invincibilityRemainingSeconds = 0.0F;
			float hitFlashRemainingSeconds = 0.0F;

			std::uint32_t killCount = 0;
			std::uint32_t deathCount = 0;

			bool isInitialized = false;
		};

		struct PlayerJoinedEvent
		{
		public:
			PlayerId playerId = 0;
			float x = 0.0F;
			float y = 0.0F;
		};

		struct RenderPlayerState
		{
		public:
			std::uint32_t playerId = 0;
			float x = 0.0F;
			float y = 0.0F;

			int hp = 0;
			bool isDead = false;

			float respawnRemainingSeconds = 0.0F;
			float invincibilityRemainingSeconds = 0.0F;
			float hitFlashRemainingSeconds = 0.0F;

			std::uint32_t killCount = 0;
			std::uint32_t deathCount = 0;

			bool isLocalPlayer = false;
		};

		struct RenderBulletState
		{
		public:
			std::uint32_t bulletId = 0;
			float x = 0.0F;
			float y = 0.0F;
		};

		struct RenderImpactEffectState
		{
		public:
			common::game::EffectType effectType = common::game::EffectType::None;
			float x = 0.0F;
			float y = 0.0F;
			float remainingSeconds = 0.0F;
		};

	public:
		using RenderPlayerStateList = std::vector<RenderPlayerState>;
		using RenderBulletStateList = std::vector<RenderBulletState>;
		using RenderImpactEffectStateList = std::vector<RenderImpactEffectState>;

	public:
		struct RenderFrameSnapshot
		{
		public:
			PlayerId localPlayerId = 0;
			RoomId currentRoomId = 0;
			std::uint32_t lastServerTick = 0;
			common::time::Milliseconds interpolationDelay{};

			RenderPlayerStateList playerStateList;
			RenderBulletStateList bulletStateList;
			RenderImpactEffectStateList impactEffectStateList;
		};

	public:
		using PlayerTable = std::unordered_map<std::uint32_t, RemotePlayerState>;
		using BulletStateDataList = std::vector<common::packet::BulletStateData>;
		using ImpactEffectDataList = std::vector<common::packet::ImpactEffectData>;

	private:
		mutable std::mutex worldMutex_;
		PlayerTable playerTable_;
		RenderBulletStateList renderBulletStateList_;
		RenderImpactEffectStateList renderImpactEffectStateList_;

		common::time::Milliseconds defaultInterpolationDelay_;
		common::time::Milliseconds minInterpolationDelay_;
		common::time::Milliseconds maxInterpolationDelay_;
		common::time::Milliseconds interpolationDelay_;

		LocalPlayerPrediction localPlayerPrediction_;
		LocalPlayerReconciliation localPlayerReconciliation_;

		PlayerId localPlayerId_ = 0;
		std::uint32_t lastServerTick_ = 0;
		RoomId currentRoomId_ = 0;
		bool isJoined_ = false;

	public:
		ClientWorld();
		~ClientWorld() noexcept = default;

		ClientWorld(const ClientWorld&) = delete;
		ClientWorld& operator=(const ClientWorld&) = delete;

		ClientWorld(ClientWorld&&) = delete;
		ClientWorld& operator=(ClientWorld&&) = delete;

	public:
		void ApplyLocalPredictionTick(std::uint32_t inputSequence, common::game::InputFlags inputFlags, float deltaSeconds);
		void ApplyPlayerJoinedEvent(const PlayerJoinedEvent& playerJoinedEvent);
		void ApplyPlayerLeftEvent(PlayerId playerId);
		void ApplyPlayerSnapshot(const common::packet::PlayerSnapshotPacket& packet);
		void ApplyBulletSnapshotData(std::uint32_t serverTick, RoomId roomId, const BulletStateDataList& bulletStateDataList);
		void ApplyImpactEffectData(std::uint32_t serverTick, RoomId roomId, const ImpactEffectDataList& impactEffectDataList);

		void UpdateLocalEffects(float deltaSeconds);

		void Clear() noexcept;
		void ResetLocalPlayerPrediction(float x, float y) noexcept;
		void SetInterpolationSettings(
			common::time::Milliseconds defaultDelay,
			common::time::Milliseconds minDelay,
			common::time::Milliseconds maxDelay
		) noexcept;

		[[nodiscard]] RenderFrameSnapshot BuildRenderFrameSnapshot(common::time::TimePoint renderTime) const;

	private:
		[[nodiscard]] RenderPlayerStateList BuildRenderPlayerStateList(common::time::TimePoint renderTime) const;

	public:
		[[nodiscard]] bool TrySetJoinState(PlayerId localPlayerId, RoomId roomId, float spawnX, float spawnY);
		void SetCurrentRoomId(RoomId roomId);
		void SetInterpolationDelay(common::time::Milliseconds interpolationDelay) noexcept;

		[[nodiscard]] bool IsJoined() const noexcept;
		[[nodiscard]] PlayerId GetLocalPlayerId() const noexcept;
		[[nodiscard]] std::uint32_t GetLastServerTick() const noexcept;
		[[nodiscard]] RoomId GetCurrentRoomId() const noexcept;
		[[nodiscard]] RenderPlayerStateList GetRenderPlayerStatesSnapshot(common::time::TimePoint renderTime) const;
		[[nodiscard]] RenderBulletStateList GetRenderBulletStatesSnapshot() const;
		[[nodiscard]] RenderImpactEffectStateList GetRenderImpactEffectStatesSnapshot() const;
		[[nodiscard]] common::time::Milliseconds GetInterpolationDelay() const noexcept;
		[[nodiscard]] bool IsLocalPlayerDead() const noexcept;

	};
}
