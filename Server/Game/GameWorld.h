#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <Common/Game/GameTypes.h>

#include <Server/Game/BulletState.h>
#include <Server/Game/ImpactEffectState.h>
#include <Server/Game/PlayerState.h>
#include <Server/Game/StatePool.h>

namespace server::game
{
	class GameWorld
	{
	public:
		using PlayerId = common::game::PlayerId;
		using BulletId = common::game::BulletId;
		using RoomId = common::game::RoomId;

		using PlayerTable = std::unordered_map<PlayerId, PlayerState>;
		using BulletStateList = std::vector<BulletState>;
		using ImpactEffectStateList = std::vector<ImpactEffectState>;

	private:
		PlayerTable playerTable_;
		StatePool<BulletState> bulletStatePool_;
		StatePool<ImpactEffectState> pendingImpactEffectStatePool_;

		PlayerId nextPlayerId_ = 1;
		BulletId nextBulletId_ = 1;
		std::uint32_t serverTick_ = 0;

	public:
		GameWorld() = default;
		~GameWorld() noexcept = default;

		GameWorld(const GameWorld&) = delete;
		GameWorld& operator=(const GameWorld&) = delete;

		GameWorld(GameWorld&&) = delete;
		GameWorld& operator=(GameWorld&&) = delete;

	public:
		void Clear() noexcept;

		[[nodiscard]] PlayerId AllocatePlayerId() noexcept;
		[[nodiscard]] BulletId AllocateBulletId() noexcept;
		void AdvanceServerTick() noexcept;

		[[nodiscard]] PlayerState* FindPlayer(PlayerId playerId) noexcept;
		[[nodiscard]] const PlayerState* FindPlayer(PlayerId playerId) const noexcept;

		[[nodiscard]] PlayerState& UpsertPlayer(PlayerState playerState);
		void RemovePlayer(PlayerId playerId) noexcept;

		BulletState& AddBullet(BulletState bulletState);
		void RemoveBullet(BulletId bulletId) noexcept;
		void RemoveBulletAt(std::size_t bulletIndex);

		ImpactEffectState& AddPendingImpactEffect(ImpactEffectState impactEffectState);

		[[nodiscard]] bool HasPendingImpactEffects() const noexcept;
		void ClearPendingImpactEffects() noexcept;

		void ClearRoomTransientState(RoomId roomId) noexcept;

	public:
		[[nodiscard]] PlayerTable& GetPlayerTable() noexcept
		{
			return playerTable_;
		}

		[[nodiscard]] const PlayerTable& GetPlayerTable() const noexcept
		{
			return playerTable_;
		}

		[[nodiscard]] BulletStateList& GetBulletStateList() noexcept
		{
			return bulletStatePool_.GetActiveStateList();
		}

		[[nodiscard]] const BulletStateList& GetBulletStateList() const noexcept
		{
			return bulletStatePool_.GetActiveStateList();
		}

		[[nodiscard]] ImpactEffectStateList& GetPendingImpactEffectStateList() noexcept
		{
			return pendingImpactEffectStatePool_.GetActiveStateList();
		}

		[[nodiscard]] const ImpactEffectStateList& GetPendingImpactEffectStateList() const noexcept
		{
			return pendingImpactEffectStatePool_.GetActiveStateList();
		}

		[[nodiscard]] std::uint32_t GetServerTick() const noexcept
		{
			return serverTick_;
		}

		[[nodiscard]] std::size_t GetPlayerCount() const noexcept
		{
			return playerTable_.size();
		}

		[[nodiscard]] std::size_t GetBulletCount() const noexcept
		{
			return bulletStatePool_.GetActiveCount();
		}

		[[nodiscard]] std::size_t GetPendingImpactEffectCount() const noexcept
		{
			return pendingImpactEffectStatePool_.GetActiveCount();
		}
	};
}