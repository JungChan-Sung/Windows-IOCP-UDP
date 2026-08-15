#include "GameWorld.h"

#include <algorithm>
#include <utility>

namespace server::game
{
	void GameWorld::Clear() noexcept
	{
		playerTable_.clear();
		bulletStatePool_.Clear();
		pendingImpactEffectStatePool_.Clear();

		nextPlayerId_ = 1;
		nextBulletId_ = 1;
		serverTick_ = 0;
	}

	GameWorld::PlayerId GameWorld::AllocatePlayerId() noexcept
	{
		return nextPlayerId_++;
	}

	GameWorld::BulletId GameWorld::AllocateBulletId() noexcept
	{
		return nextBulletId_++;
	}

	void GameWorld::AdvanceServerTick() noexcept
	{
		++serverTick_;
	}

	PlayerState* GameWorld::FindPlayer(PlayerId playerId) noexcept
	{
		const auto playerIterator = playerTable_.find(playerId);
		if (playerIterator == playerTable_.end())
		{
			return nullptr;
		}

		return &playerIterator->second;
	}

	const PlayerState* GameWorld::FindPlayer(PlayerId playerId) const noexcept
	{
		const auto playerIterator = playerTable_.find(playerId);
		if (playerIterator == playerTable_.end())
		{
			return nullptr;
		}

		return &playerIterator->second;
	}

	PlayerState& GameWorld::UpsertPlayer(PlayerState playerState)
	{
		const PlayerId playerId = playerState.playerId;

		auto [playerIterator, _] = playerTable_.insert_or_assign(playerId, std::move(playerState));

		return playerIterator->second;
	}

	void GameWorld::RemovePlayer(PlayerId playerId) noexcept
	{
		playerTable_.erase(playerId);
	}

	BulletState& GameWorld::AddBullet(BulletState bulletState)
	{
		return bulletStatePool_.Add(std::move(bulletState));
	}

	void GameWorld::RemoveBullet(BulletId bulletId) noexcept
	{
		BulletStateList& bulletStateList = bulletStatePool_.GetActiveStateList();

		const auto bulletIterator = std::ranges::find_if(
			bulletStateList,
			[bulletId](const BulletState& bulletState)
			{
				return bulletState.bulletId == bulletId;
			}
		);

		if (bulletIterator == bulletStateList.end())
		{
			return;
		}

		const std::size_t bulletIndex = static_cast<std::size_t>(std::distance(bulletStateList.begin(), bulletIterator));
		RemoveBulletAt(bulletIndex);
	}

	void GameWorld::RemoveBulletAt(std::size_t bulletIndex)
	{
		bulletStatePool_.RemoveAt(bulletIndex);
	}

	ImpactEffectState& GameWorld::AddPendingImpactEffect(ImpactEffectState impactEffectState)
	{
		return pendingImpactEffectStatePool_.Add(std::move(impactEffectState));
	}

	bool GameWorld::HasPendingImpactEffects() const noexcept
	{
		return pendingImpactEffectStatePool_.GetActiveCount() > 0;
	}

	void GameWorld::ClearPendingImpactEffects() noexcept
	{
		pendingImpactEffectStatePool_.RecycleAll();
	}

	void GameWorld::ClearRoomTransientState(RoomId roomId) noexcept
	{
		BulletStateList& bulletStateList = bulletStatePool_.GetActiveStateList();
		for (std::size_t bulletIndex = 0; bulletIndex < bulletStateList.size();)
		{
			if (bulletStateList[bulletIndex].roomId != roomId)
			{
				++bulletIndex;
				continue;
			}

			bulletStatePool_.RemoveAt(bulletIndex);
		}

		ImpactEffectStateList& impactEffectStateList = pendingImpactEffectStatePool_.GetActiveStateList();
		for (std::size_t effectIndex = 0; effectIndex < impactEffectStateList.size();)
		{
			if (impactEffectStateList[effectIndex].roomId != roomId)
			{
				++effectIndex;
				continue;
			}

			pendingImpactEffectStatePool_.RemoveAt(effectIndex);
		}
	}
}