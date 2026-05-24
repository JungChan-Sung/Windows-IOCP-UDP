#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <Common/Packet/GamePacket.h>
#include <Common/Game/GameTypes.h>
#include <Common/Game/InputFlags.h>

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
			std::chrono::steady_clock::time_point time{};
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

		struct PendingInput
		{
			std::uint32_t sequence = 0;
			common::game::InputFlags inputFlags{};
			float deltaSeconds = 0.0F;
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
			common::packet::EffectType effectType = common::packet::EffectType::None;
			float x = 0.0F;
			float y = 0.0F;
			float remainingSeconds = 0.0F;
		};

	public:
		using PlayerTable = std::unordered_map<std::uint32_t, RemotePlayerState>;
		using PendingInputList = std::deque<PendingInput>;
		using RenderPlayerStateList = std::vector<RenderPlayerState>;
		using RenderBulletStateList = std::vector<RenderBulletState>;
		using RenderImpactEffectStateList = std::vector<RenderImpactEffectState>;
		using BulletStateDataList = std::vector<common::packet::BulletStateData>;
		using ImpactEffectDataList = std::vector<common::packet::ImpactEffectData>;

	private:
		static inline constexpr float localCorrectionIgnoreDistance = 2.0F;
		static inline constexpr float localCorrectionHardSnapDistance = 160.0F;
		static inline constexpr float localRenderCorrectionMaxOffset = 96.0F;
		static inline constexpr float localRenderCorrectionSmoothSpeed = 12.0F;
		static inline constexpr float localRenderCorrectionClearDistance = 0.25F;

	private:
		mutable std::mutex worldMutex_;
		PlayerTable playerTable_;
		PendingInputList pendingInputList_;
		RenderBulletStateList renderBulletStateList_;
		RenderImpactEffectStateList renderImpactEffectStateList_;

		std::chrono::milliseconds defaultInterpolationDelay_;
		std::chrono::milliseconds minInterpolationDelay_;
		std::chrono::milliseconds maxInterpolationDelay_;
		std::chrono::milliseconds interpolationDelay_;

		float localPredictedX_ = 0.0F;
		float localPredictedY_ = 0.0F;
		float localRenderCorrectionOffsetX_ = 0.0F;
		float localRenderCorrectionOffsetY_ = 0.0F;
		bool isLocalPredictedInitialized_ = false;

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
		void ApplyBulletSnapshotData(
			std::uint32_t serverTick, 
			RoomId roomId,
			const BulletStateDataList& bulletStateDataList
		);
		void ApplyImpactEffectData(
			std::uint32_t serverTick,
			RoomId roomId,
			const ImpactEffectDataList& impactEffectDataList
		);

		void UpdateLocalEffects(float deltaSeconds);

		void Clear() noexcept;
		void ResetLocalPlayerPrediction(float x, float y) noexcept;
		void SetInterpolationSettings(
			std::chrono::milliseconds defaultDelay,
			std::chrono::milliseconds minDelay,
			std::chrono::milliseconds maxDelay
		) noexcept;

	public:
		void SetJoinState(PlayerId localPlayerId, RoomId roomId, float spawnX, float spawnY);
		void SetCurrentRoomId(RoomId roomId);
		void SetInterpolationDelay(std::chrono::milliseconds interpolationDelay) noexcept;

		[[nodiscard]] bool IsJoined() const noexcept;
		[[nodiscard]] PlayerId GetLocalPlayerId() const noexcept;
		[[nodiscard]] std::uint32_t GetLastServerTick() const noexcept;
		[[nodiscard]] RoomId GetCurrentRoomId() const noexcept;
		[[nodiscard]] RenderPlayerStateList GetRenderPlayerStatesSnapshot(
			std::chrono::steady_clock::time_point renderTime
		) const;
		[[nodiscard]] RenderBulletStateList GetRenderBulletStatesSnapshot() const;
		[[nodiscard]] RenderImpactEffectStateList GetRenderImpactEffectStatesSnapshot() const;
		[[nodiscard]] std::chrono::milliseconds GetInterpolationDelay() const noexcept;
		[[nodiscard]] bool IsLocalPlayerDead() const noexcept;
	};
}
