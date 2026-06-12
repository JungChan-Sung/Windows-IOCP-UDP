#include "ClientWorld.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include <Common/Packet/GamePacket.h>
#include <Common/Game/SimulationConstants.h>
#include <Common/Game/RoomLayout.h>
#include <Common/Game/WorldCollision.h>

#include <Client/Game/ClientTuning.h>

#include "EffectConfig.h"

namespace
{
	using SnapshotSample = client::game::ClientWorld::SnapshotSample;
	using RemotePlayerState = client::game::ClientWorld::RemotePlayerState;
	using PendingInput = client::game::ClientWorld::PendingInput;

	[[nodiscard]] float Lerp(float startValue, float endValue, float alpha) noexcept
	{
		return startValue + ((endValue - startValue) * alpha);
	}

	[[nodiscard]] float LengthSquared(float x, float y) noexcept
	{
		return (x * x) + (y * y);
	}

	void ClampVectorLength(float& x, float& y, float maxLength) noexcept
	{
		const float lengthSquared = LengthSquared(x, y);
		const float maxLengthSquared = maxLength * maxLength;

		if (lengthSquared <= maxLengthSquared)
		{
			return;
		}

		const float length = std::sqrt(lengthSquared);
		if (length <= 0.0F)
		{
			x = 0.0F;
			y = 0.0F;
			return;
		}

		const float scale = maxLength / length;
		x *= scale;
		y *= scale;
	}

	void InitializePlayerState(
		RemotePlayerState& playerState,
		std::uint32_t playerId,
		float x,
		float y,
		std::chrono::steady_clock::time_point sampleTime
	) noexcept
	{
		playerState.playerId = playerId;
		playerState.previousSample.x = x;
		playerState.previousSample.y = y;
		playerState.previousSample.time = sampleTime;

		playerState.targetSample.x = x;
		playerState.targetSample.y = y;
		playerState.targetSample.time = sampleTime;

		playerState.hp = 0;
		playerState.isDead = false;
		playerState.respawnRemainingSeconds = 0.0F;
		playerState.invincibilityRemainingSeconds = 0.0F;
		playerState.killCount = 0;
		playerState.deathCount = 0;
		playerState.isInitialized = true;
	}

	void UpdatePlayerSample(
		RemotePlayerState& playerState,
		float x,
		float y,
		std::chrono::steady_clock::time_point sampleTime
	) noexcept
	{
		playerState.previousSample = playerState.targetSample;
		playerState.targetSample.x = x;
		playerState.targetSample.y = y;
		playerState.targetSample.time = sampleTime;
	}
}

namespace client::game
{
	ClientWorld::ClientWorld()
		: defaultInterpolationDelay_(game::defaultInterpolationDelay),
		minInterpolationDelay_(game::minInterpolationDelay),
		maxInterpolationDelay_(game::maxInterpolationDelay),
		interpolationDelay_(game::defaultInterpolationDelay)
	{}

	void ClientWorld::ApplyLocalPredictionTick(std::uint32_t inputSequence, common::game::InputFlags inputFlags, float deltaSeconds)
	{
		std::scoped_lock lock(worldMutex_);

		if (!isJoined_ || localPlayerId_ == 0 || !isLocalPredictedInitialized_)
		{
			return;
		}

		PendingInput pendingInput{};
		pendingInput.sequence = inputSequence;
		pendingInput.inputFlags = inputFlags;
		pendingInput.deltaSeconds = deltaSeconds;
		pendingInputList_.push_back(pendingInput);

		common::game::MovePlayerWithWallCollision(
			localPredictedX_,
			localPredictedY_,
			inputFlags,
			deltaSeconds,
			common::game::defaultMoveSpeed,
			common::game::playerHalfExtent,
			common::game::defaultWorldBounds,
			common::game::GetWallRectListForRoom(currentRoomId_)
		);
	}

	void ClientWorld::ApplyPlayerJoinedEvent(const PlayerJoinedEvent& playerJoinedEvent)
	{
		std::scoped_lock lock(worldMutex_);

		const auto currentTime = std::chrono::steady_clock::now();

		RemotePlayerState& playerState = playerTable_[playerJoinedEvent.playerId];

		if (!playerState.isInitialized)
		{
			InitializePlayerState(
				playerState,
				playerJoinedEvent.playerId,
				playerJoinedEvent.x,
				playerJoinedEvent.y,
				currentTime
			);
		}
		else
		{
			UpdatePlayerSample(
				playerState,
				playerJoinedEvent.x,
				playerJoinedEvent.y,
				currentTime
			);
		}

		if (playerJoinedEvent.playerId == localPlayerId_)
		{
			localPredictedX_ = playerJoinedEvent.x;
			localPredictedY_ = playerJoinedEvent.y;
			localRenderCorrectionOffsetX_ = 0.0F;
			localRenderCorrectionOffsetY_ = 0.0F;
			isLocalPredictedInitialized_ = true;
		}
	}

	void ClientWorld::ApplyPlayerLeftEvent(PlayerId playerId)
	{
		std::scoped_lock lock(worldMutex_);
		playerTable_.erase(playerId);
	}

	void ClientWorld::ApplyPlayerSnapshot(const common::packet::PlayerSnapshotPacket& packet)
	{
		std::scoped_lock lock(worldMutex_);

		lastServerTick_ = packet.serverTick;
		currentRoomId_ = packet.roomId;

		const auto currentTime = std::chrono::steady_clock::now();
		const std::size_t playerCount = std::min(
			static_cast<std::size_t>(packet.playerCount),
			packet.players.size()
		);

		std::unordered_set<std::uint32_t> receivedPlayerIdSet;
		receivedPlayerIdSet.reserve(playerCount);

		bool hasLocalAuthoritativeState = false;
		float localAuthoritativeX = 0.0F;
		float localAuthoritativeY = 0.0F;

		for (std::size_t index = 0; index < playerCount; ++index)
		{
			const common::packet::PlayerStateData& playerStateData = packet.players[index];
			receivedPlayerIdSet.insert(playerStateData.playerId);

			RemotePlayerState& playerState = playerTable_[playerStateData.playerId];

			if (!playerState.isInitialized)
			{
				InitializePlayerState(
					playerState,
					playerStateData.playerId,
					playerStateData.x,
					playerStateData.y,
					currentTime
				);
			}
			else
			{
				UpdatePlayerSample(
					playerState,
					playerStateData.x,
					playerStateData.y,
					currentTime
				);
			}

			playerState.hp = playerStateData.hp;
			playerState.isDead = playerStateData.isDead != 0;
			playerState.respawnRemainingSeconds = playerStateData.respawnRemainingSeconds;
			playerState.invincibilityRemainingSeconds = playerStateData.invincibilityRemainingSeconds;
			playerState.hitFlashRemainingSeconds = playerStateData.hitFlashRemainingSeconds;
			playerState.killCount = playerStateData.killCount;
			playerState.deathCount = playerStateData.deathCount;

			if (playerStateData.playerId == localPlayerId_)
			{
				hasLocalAuthoritativeState = true;
				localAuthoritativeX = playerStateData.x;
				localAuthoritativeY = playerStateData.y;
			}
		}

		for (auto playerIterator = playerTable_.begin(); playerIterator != playerTable_.end();)
		{
			if (receivedPlayerIdSet.contains(playerIterator->first))
			{
				++playerIterator;
				continue;
			}

			playerIterator = playerTable_.erase(playerIterator);
		}

		if (!hasLocalAuthoritativeState)
		{
			return;
		}

		while (!pendingInputList_.empty() && pendingInputList_.front().sequence <= packet.lastProcessedInputSequence)
		{
			pendingInputList_.pop_front();
		}

		float reconciledX = localAuthoritativeX;
		float reconciledY = localAuthoritativeY;

		for (const PendingInput& pendingInput : pendingInputList_)
		{
			common::game::MovePlayerWithWallCollision(
				reconciledX,
				reconciledY,
				pendingInput.inputFlags,
				pendingInput.deltaSeconds,
				common::game::defaultMoveSpeed,
				common::game::playerHalfExtent,
				common::game::defaultWorldBounds,
				common::game::GetWallRectListForRoom(packet.roomId)
			);
		}

		if (!isLocalPredictedInitialized_)
		{
			localPredictedX_ = reconciledX;
			localPredictedY_ = reconciledY;
			localRenderCorrectionOffsetX_ = 0.0F;
			localRenderCorrectionOffsetY_ = 0.0F;
			isLocalPredictedInitialized_ = true;
			return;
		}

		const float oldPredictedX = localPredictedX_;
		const float oldPredictedY = localPredictedY_;

		const float correctionDeltaX = reconciledX - oldPredictedX;
		const float correctionDeltaY = reconciledY - oldPredictedY;
		const float correctionDistanceSquared = LengthSquared(correctionDeltaX, correctionDeltaY);

		const float ignoreDistanceSquared = localCorrectionIgnoreDistance * localCorrectionIgnoreDistance;
		const float hardSnapDistanceSquared = localCorrectionHardSnapDistance * localCorrectionHardSnapDistance;

		if (correctionDistanceSquared <= ignoreDistanceSquared)
		{
			return;
		}

		localPredictedX_ = reconciledX;
		localPredictedY_ = reconciledY;

		if (correctionDistanceSquared >= hardSnapDistanceSquared)
		{
			localRenderCorrectionOffsetX_ = 0.0F;
			localRenderCorrectionOffsetY_ = 0.0F;
			return;
		}

		localRenderCorrectionOffsetX_ += oldPredictedX - reconciledX;
		localRenderCorrectionOffsetY_ += oldPredictedY - reconciledY;

		ClampVectorLength(
			localRenderCorrectionOffsetX_,
			localRenderCorrectionOffsetY_,
			localRenderCorrectionMaxOffset
		);
	}

	void ClientWorld::ApplyBulletSnapshotData(std::uint32_t serverTick, RoomId roomId, const BulletStateDataList& bulletStateDataList)
	{
		std::scoped_lock lock(worldMutex_);

		(void)serverTick;

		if (currentRoomId_ != 0 && roomId != currentRoomId_)
		{
			return;
		}

		renderBulletStateList_.clear();
		renderBulletStateList_.reserve(bulletStateDataList.size());

		for (const common::packet::BulletStateData& bulletStateData : bulletStateDataList)
		{
			RenderBulletState renderBulletState{};
			renderBulletState.bulletId = bulletStateData.bulletId;
			renderBulletState.x = bulletStateData.x;
			renderBulletState.y = bulletStateData.y;

			renderBulletStateList_.push_back(renderBulletState);
		}
	}

	void ClientWorld::ApplyImpactEffectData(std::uint32_t serverTick, RoomId roomId, const ImpactEffectDataList& impactEffectDataList)
	{
		std::scoped_lock lock(worldMutex_);

		(void)serverTick;

		if (currentRoomId_ != 0 && roomId != currentRoomId_)
		{
			return;
		}

		for (const common::packet::ImpactEffectData& impactEffectData : impactEffectDataList)
		{
			RenderImpactEffectState renderImpactEffectState{};
			renderImpactEffectState.effectType = impactEffectData.effectType;
			renderImpactEffectState.x = impactEffectData.x;
			renderImpactEffectState.y = impactEffectData.y;
			renderImpactEffectState.remainingSeconds = GetEffectDurationSeconds(impactEffectData.effectType);

			renderImpactEffectStateList_.push_back(renderImpactEffectState);
		}
	}

	void ClientWorld::UpdateLocalEffects(float deltaSeconds)
	{
		std::scoped_lock lock(worldMutex_);

		for (auto effectIterator = renderImpactEffectStateList_.begin(); effectIterator != renderImpactEffectStateList_.end();)
		{
			effectIterator->remainingSeconds -= deltaSeconds;

			if (effectIterator->remainingSeconds > 0.0F)
			{
				++effectIterator;
				continue;
			}

			effectIterator = renderImpactEffectStateList_.erase(effectIterator);
		}

		const float correctionOffsetDistanceSquared = LengthSquared(
			localRenderCorrectionOffsetX_,
			localRenderCorrectionOffsetY_
		);

		if (correctionOffsetDistanceSquared > 0.0F)
		{
			const float alpha = std::clamp(localRenderCorrectionSmoothSpeed * deltaSeconds, 0.0F, 1.0F);

			localRenderCorrectionOffsetX_ = Lerp(localRenderCorrectionOffsetX_, 0.0F, alpha);
			localRenderCorrectionOffsetY_ = Lerp(localRenderCorrectionOffsetY_, 0.0F, alpha);

			const float clearDistanceSquared =
				localRenderCorrectionClearDistance * localRenderCorrectionClearDistance;

			if (LengthSquared(localRenderCorrectionOffsetX_, localRenderCorrectionOffsetY_) <= clearDistanceSquared)
			{
				localRenderCorrectionOffsetX_ = 0.0F;
				localRenderCorrectionOffsetY_ = 0.0F;
			}
		}
	}

	void ClientWorld::Clear() noexcept
	{
		std::scoped_lock lock(worldMutex_);

		playerTable_.clear();
		pendingInputList_.clear();
		renderBulletStateList_.clear();
		renderImpactEffectStateList_.clear();

		interpolationDelay_ = defaultInterpolationDelay_;

		localPredictedX_ = 0.0F;
		localPredictedY_ = 0.0F;
		localRenderCorrectionOffsetX_ = 0.0F;
		localRenderCorrectionOffsetY_ = 0.0F;
		isLocalPredictedInitialized_ = false;

		localPlayerId_ = 0;
		lastServerTick_ = 0;
		currentRoomId_ = 0;
		isJoined_ = false;
	}

	void ClientWorld::ResetLocalPlayerPrediction(float x, float y) noexcept
	{
		std::scoped_lock lock(worldMutex_);

		localPredictedX_ = x;
		localPredictedY_ = y;
		localRenderCorrectionOffsetX_ = 0.0F;
		localRenderCorrectionOffsetY_ = 0.0F;
		isLocalPredictedInitialized_ = true;
		pendingInputList_.clear();

		auto playerIterator = playerTable_.find(localPlayerId_);
		if (playerIterator != playerTable_.end())
		{
			const auto currentTime = std::chrono::steady_clock::now();

			playerIterator->second.previousSample.x = x;
			playerIterator->second.previousSample.y = y;
			playerIterator->second.previousSample.time = currentTime;

			playerIterator->second.targetSample.x = x;
			playerIterator->second.targetSample.y = y;
			playerIterator->second.targetSample.time = currentTime;

			playerIterator->second.isInitialized = true;
		}
	}

	void ClientWorld::SetInterpolationSettings(std::chrono::milliseconds defaultDelay, std::chrono::milliseconds minDelay, std::chrono::milliseconds maxDelay) noexcept
	{
		std::scoped_lock lock(worldMutex_);

		minInterpolationDelay_ = minDelay;
		maxInterpolationDelay_ = maxDelay;
		defaultInterpolationDelay_ = std::clamp(defaultDelay, minInterpolationDelay_, maxInterpolationDelay_);
		interpolationDelay_ = defaultInterpolationDelay_;
	}

	bool ClientWorld::TrySetJoinState(PlayerId localPlayerId, RoomId roomId, float spawnX, float spawnY)
	{
		std::scoped_lock lock(worldMutex_);

		if (isJoined_ || localPlayerId == 0 || roomId <= 0)
		{
			return false;
		}

		localPlayerId_ = localPlayerId;
		currentRoomId_ = roomId;
		isJoined_ = true;

		localPredictedX_ = spawnX;
		localPredictedY_ = spawnY;
		isLocalPredictedInitialized_ = true;
		pendingInputList_.clear();

		return true;
	}

	void ClientWorld::SetCurrentRoomId(RoomId roomId)
	{
		std::scoped_lock lock(worldMutex_);
		currentRoomId_ = roomId;
	}


	void ClientWorld::SetInterpolationDelay(std::chrono::milliseconds interpolationDelay) noexcept
	{
		std::scoped_lock lock(worldMutex_);

		if (interpolationDelay < minInterpolationDelay_)
		{
			interpolationDelay_ = minInterpolationDelay_;
			return;
		}

		if (interpolationDelay > maxInterpolationDelay_)
		{
			interpolationDelay_ = maxInterpolationDelay_;
			return;
		}

		interpolationDelay_ = interpolationDelay;
	}

	bool ClientWorld::IsJoined() const noexcept
	{
		std::scoped_lock lock(worldMutex_);
		return isJoined_;
	}

	ClientWorld::PlayerId ClientWorld::GetLocalPlayerId() const noexcept
	{
		std::scoped_lock lock(worldMutex_);
		return localPlayerId_;
	}

	std::uint32_t ClientWorld::GetLastServerTick() const noexcept
	{
		std::scoped_lock lock(worldMutex_);
		return lastServerTick_;
	}

	ClientWorld::RoomId ClientWorld::GetCurrentRoomId() const noexcept
	{
		std::scoped_lock lock(worldMutex_);
		return currentRoomId_;
	}

	ClientWorld::RenderPlayerStateList ClientWorld::GetRenderPlayerStatesSnapshot(std::chrono::steady_clock::time_point renderTime) const
	{
		std::scoped_lock lock(worldMutex_);

		RenderPlayerStateList renderPlayerStateList;
		renderPlayerStateList.reserve(playerTable_.size());

		const auto interpolationTargetTime = renderTime - interpolationDelay_;

		for (const auto& playerEntry : playerTable_)
		{
			const RemotePlayerState& playerState = playerEntry.second;
			if (!playerState.isInitialized)
			{
				continue;
			}

			RenderPlayerState renderPlayerState{};
			renderPlayerState.playerId = playerState.playerId;
			renderPlayerState.isLocalPlayer = playerState.playerId == localPlayerId_;
			renderPlayerState.hp = playerState.hp;
			renderPlayerState.isDead = playerState.isDead;
			renderPlayerState.respawnRemainingSeconds = playerState.respawnRemainingSeconds;
			renderPlayerState.invincibilityRemainingSeconds = playerState.invincibilityRemainingSeconds;
			renderPlayerState.hitFlashRemainingSeconds = playerState.hitFlashRemainingSeconds;
			renderPlayerState.killCount = playerState.killCount;
			renderPlayerState.deathCount = playerState.deathCount;

			if (renderPlayerState.isLocalPlayer)
			{
				if (isLocalPredictedInitialized_)
				{
					renderPlayerState.x = localPredictedX_ + localRenderCorrectionOffsetX_;
					renderPlayerState.y = localPredictedY_ + localRenderCorrectionOffsetY_;
				}
				else
				{
					renderPlayerState.x = playerState.targetSample.x;
					renderPlayerState.y = playerState.targetSample.y;
				}

				renderPlayerStateList.push_back(renderPlayerState);
				continue;
			}

			const SnapshotSample& previousSample = playerState.previousSample;
			const SnapshotSample& targetSample = playerState.targetSample;

			if (targetSample.time <= previousSample.time)
			{
				renderPlayerState.x = targetSample.x;
				renderPlayerState.y = targetSample.y;

				renderPlayerStateList.push_back(renderPlayerState);
				continue;
			}

			const float totalSeconds = std::chrono::duration<float>(targetSample.time - previousSample.time).count();
			const float elapsedSeconds = std::chrono::duration<float>(interpolationTargetTime - previousSample.time).count();
			const float alpha = std::clamp(elapsedSeconds / totalSeconds, 0.0F, 1.0F);

			renderPlayerState.x = Lerp(previousSample.x, targetSample.x, alpha);
			renderPlayerState.y = Lerp(previousSample.y, targetSample.y, alpha);

			renderPlayerStateList.push_back(renderPlayerState);
		}

		return renderPlayerStateList;
	}

	ClientWorld::RenderBulletStateList ClientWorld::GetRenderBulletStatesSnapshot() const
	{
		std::scoped_lock lock(worldMutex_);
		return renderBulletStateList_;
	}

	ClientWorld::RenderImpactEffectStateList ClientWorld::GetRenderImpactEffectStatesSnapshot() const
	{
		std::scoped_lock lock(worldMutex_);
		return renderImpactEffectStateList_;
	}

	std::chrono::milliseconds ClientWorld::GetInterpolationDelay() const noexcept
	{
		std::scoped_lock lock(worldMutex_);
		return interpolationDelay_;
	}

	bool ClientWorld::IsLocalPlayerDead() const noexcept
	{
		std::scoped_lock lock(worldMutex_);

		if (localPlayerId_ == 0)
		{
			return false;
		}

		const auto playerIterator = playerTable_.find(localPlayerId_);
		if (playerIterator == playerTable_.end())
		{
			return false;
		}

		return playerIterator->second.isDead;
	}
}