#include "ClientWorld.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include <Common/Game/SimulationConstants.h>
#include <Common/Net/SequenceNumber.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Time/TimeTypes.h>

#include <Client/Game/ClientTuning.h>
#include <Client/Game/EffectConfig.h>

namespace
{
	using RemotePlayerState = client::game::ClientWorld::RemotePlayerState;

	void InitializePlayerState(RemotePlayerState& playerState, std::uint32_t playerId, float x, float y, common::time::TimePoint sampleTime)
	{
		playerState.playerId = playerId;
		playerState.interpolationBuffer.Reset(x, y, sampleTime);

		playerState.hp = 0;
		playerState.isDead = false;
		playerState.respawnRemainingSeconds = 0.0F;
		playerState.invincibilityRemainingSeconds = 0.0F;
		playerState.killCount = 0;
		playerState.deathCount = 0;
		playerState.isInitialized = true;
	}

	void UpdatePlayerSample(RemotePlayerState& playerState, float x, float y, common::time::TimePoint sampleTime)
	{
		playerState.interpolationBuffer.PushSample(x, y, sampleTime);
	}
}

namespace client::game
{
	void ClientWorld::ApplyLocalPredictionTick(std::uint32_t inputSequence, common::game::InputFlags inputFlags, float deltaSeconds)
	{
		std::scoped_lock lock(worldMutex_);

		if (!isJoined_ || localPlayerId_ == 0 || !localPlayerPrediction_.IsInitialized() || deltaSeconds <= 0.0F)
		{
			return;
		}

		const auto localPlayerIterator = playerTable_.find(localPlayerId_);
		if (localPlayerIterator != playerTable_.end() && localPlayerIterator->second.isDead)
		{
			return;
		}

		localPlayerReconciliation_.RecordPendingInput(inputSequence, inputFlags, deltaSeconds);

		localPlayerPrediction_.ApplyInput(inputFlags, deltaSeconds, common::game::defaultMoveSpeed, currentRoomId_);
	}

	void ClientWorld::ApplyPlayerJoinedEvent(const PlayerJoinedEvent& playerJoinedEvent)
	{
		std::scoped_lock lock(worldMutex_);

		const auto currentTime = common::time::Clock::now();

		RemotePlayerState& playerState = playerTable_[playerJoinedEvent.playerId];

		if (!playerState.isInitialized)
		{
			InitializePlayerState(playerState, playerJoinedEvent.playerId, playerJoinedEvent.x, playerJoinedEvent.y, currentTime);
		}
		else
		{
			UpdatePlayerSample(playerState, playerJoinedEvent.x, playerJoinedEvent.y, currentTime);
		}

		if (playerJoinedEvent.playerId == localPlayerId_)
		{
			localPlayerPrediction_.Reset(playerJoinedEvent.x, playerJoinedEvent.y);
			localPlayerReconciliation_.Clear();
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

		if (currentRoomId_ != 0 && packet.roomId != currentRoomId_)
		{
			return;
		}

		if (hasReceivedPlayerSnapshot_ && !common::net::IsSequenceNewer(packet.serverTick, lastServerTick_))
		{
			return;
		}

		lastServerTick_ = packet.serverTick;
		currentRoomId_ = packet.roomId;
		hasReceivedPlayerSnapshot_ = true;

		const auto currentTime = common::time::Clock::now();
		const common::time::Milliseconds serverTickInterval(packet.serverTickIntervalMilliseconds);
		const ServerTickTimeline::SampleTimeResult sampleTimeResult = serverTickTimeline_.ResolveSampleTime(
			packet.serverTick,
			serverTickInterval,
			currentTime
		);
		const common::time::TimePoint snapshotSampleTime = sampleTimeResult.sampleTime;
		interpolationDelayController_.ObserveSnapshotTiming(
			currentTime,
			snapshotSampleTime,
			serverTickInterval
		);

		const common::time::TimePoint minimumInterpolationTargetTime = snapshotSampleTime - interpolationDelayController_.GetMaxDelay();

		const std::size_t playerCount = std::min(static_cast<std::size_t>(packet.playerCount), packet.players.size());

		std::unordered_set<std::uint32_t> receivedPlayerIdSet;
		receivedPlayerIdSet.reserve(playerCount);

		bool hasLocalAuthoritativeState = false;
		bool hasLocalPlayerLifecycleTransition = false;

		float localAuthoritativeX = 0.0F;
		float localAuthoritativeY = 0.0F;

		for (std::size_t index = 0; index < playerCount; ++index)
		{
			const common::packet::PlayerStateData& playerStateData = packet.players[index];
			receivedPlayerIdSet.insert(playerStateData.playerId);

			RemotePlayerState& playerState = playerTable_[playerStateData.playerId];

			const bool wasInitialized = playerState.isInitialized;
			const bool wasDead = playerState.isDead;
			const bool isDead = playerStateData.isDead != 0;

			if (!playerState.isInitialized)
			{
				InitializePlayerState(playerState, playerStateData.playerId, playerStateData.x, playerStateData.y, snapshotSampleTime);
			}
			else if (shouldReanchorNextPlayerSnapshot_ || sampleTimeResult.wasReanchored || wasDead != isDead)
			{
				playerState.interpolationBuffer.Reset(playerStateData.x, playerStateData.y, snapshotSampleTime);
			}
			else
			{
				UpdatePlayerSample(playerState, playerStateData.x, playerStateData.y, snapshotSampleTime);
			}

			playerState.interpolationBuffer.PruneBefore(minimumInterpolationTargetTime);

			playerState.hp = playerStateData.hp;
			playerState.isDead = isDead;
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

				if (wasInitialized && wasDead != isDead)
				{
					hasLocalPlayerLifecycleTransition = true;
				}
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

		shouldReanchorNextPlayerSnapshot_ = false;

		if (!hasLocalAuthoritativeState)
		{
			return;
		}

		if (hasLocalPlayerLifecycleTransition)
		{
			localPlayerPrediction_.Reset(localAuthoritativeX, localAuthoritativeY);
			localPlayerReconciliation_.Clear();
			return;
		}

		localPlayerReconciliation_.Reconcile(
			localPlayerPrediction_,
			localAuthoritativeX,
			localAuthoritativeY,
			packet.lastProcessedInputSequence,
			common::game::defaultMoveSpeed,
			packet.roomId
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

		localPlayerReconciliation_.UpdateRenderCorrection(deltaSeconds);
	}

	void ClientWorld::BeginRecovery() noexcept
	{
		std::scoped_lock lock(worldMutex_);

		if (!isJoined_)
		{
			return;
		}

		isJoined_ = false;

		localPlayerReconciliation_.Clear();
	}

	void ClientWorld::Clear() noexcept
	{
		std::scoped_lock lock(worldMutex_);

		playerTable_.clear();
		renderBulletStateList_.clear();
		renderImpactEffectStateList_.clear();

		interpolationDelayController_.Reset();

		localPlayerPrediction_.Clear();
		localPlayerReconciliation_.Clear();
		serverTickTimeline_.Clear();

		localPlayerId_ = 0;
		lastServerTick_ = 0;
		currentRoomId_ = 0;

		hasReceivedPlayerSnapshot_ = false;
		shouldReanchorNextPlayerSnapshot_ = false;
		isJoined_ = false;
	}

	void ClientWorld::ResetLocalPlayerPrediction(float x, float y)
	{
		std::scoped_lock lock(worldMutex_);

		localPlayerPrediction_.Reset(x, y);
		localPlayerReconciliation_.Clear();

		auto playerIterator = playerTable_.find(localPlayerId_);
		if (playerIterator != playerTable_.end())
		{
			const auto currentTime = common::time::Clock::now();

			playerIterator->second.interpolationBuffer.Reset(x, y, currentTime);
			playerIterator->second.isInitialized = true;
		}
	}

	void ClientWorld::SetInterpolationSettings(common::time::Milliseconds defaultDelay, common::time::Milliseconds minDelay, common::time::Milliseconds maxDelay) noexcept
	{
		std::scoped_lock lock(worldMutex_);

		interpolationDelayController_.Configure(defaultDelay, minDelay, maxDelay);
	}

	ClientWorld::RenderFrameSnapshot ClientWorld::BuildRenderFrameSnapshot(common::time::TimePoint renderTime) const
	{
		std::scoped_lock lock(worldMutex_);

		RenderFrameSnapshot snapshot{};
		snapshot.localPlayerId = localPlayerId_;
		snapshot.currentRoomId = currentRoomId_;
		snapshot.lastServerTick = lastServerTick_;

		snapshot.interpolationEnabled = interpolationEnabled_;
		snapshot.interpolationDelay = interpolationDelayController_.GetDelay();

		snapshot.playerStateList = BuildRenderPlayerStateList(renderTime);
		snapshot.bulletStateList = renderBulletStateList_;
		snapshot.impactEffectStateList = renderImpactEffectStateList_;

		return snapshot;
	}

	ClientWorld::RenderPlayerStateList ClientWorld::BuildRenderPlayerStateList(common::time::TimePoint renderTime) const
	{
		RenderPlayerStateList renderPlayerStateList;
		renderPlayerStateList.reserve(playerTable_.size());

		const common::time::TimePoint interpolationTargetTime = renderTime - interpolationDelayController_.GetDelay();

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
				if (localPlayerPrediction_.IsInitialized())
				{
					renderPlayerState.x = localPlayerPrediction_.GetX() + localPlayerReconciliation_.GetRenderCorrectionOffsetX();
					renderPlayerState.y = localPlayerPrediction_.GetY() + localPlayerReconciliation_.GetRenderCorrectionOffsetY();
				}
				else
				{
					const auto latestPosition = playerState.interpolationBuffer.GetLatestPosition();
					if (latestPosition.has_value())
					{
						renderPlayerState.x = latestPosition->x;
						renderPlayerState.y = latestPosition->y;
					}
				}

				renderPlayerStateList.push_back(renderPlayerState);
				continue;
			}

			const auto renderPosition = interpolationEnabled_
				? playerState.interpolationBuffer.Interpolate(interpolationTargetTime) 
				: playerState.interpolationBuffer.GetLatestPosition();
			if (!renderPosition.has_value())
			{
				continue;
			}

			renderPlayerState.x = renderPosition->x;
			renderPlayerState.y = renderPosition->y;

			renderPlayerStateList.push_back(renderPlayerState);
		}

		return renderPlayerStateList;
	}

	bool ClientWorld::TrySetJoinState(PlayerId localPlayerId, RoomId roomId, float spawnX, float spawnY)
	{
		std::scoped_lock lock(worldMutex_);

		if (isJoined_ || localPlayerId == 0 || roomId <= 0)
		{
			return false;
		}

		const bool isRecoveryJoin = localPlayerId_ != 0;
		if (isRecoveryJoin && localPlayerId_ != localPlayerId)
		{
			return false;
		}

		localPlayerId_ = localPlayerId;
		currentRoomId_ = roomId;
		isJoined_ = true;

		localPlayerPrediction_.Reset(spawnX, spawnY);
		localPlayerReconciliation_.Clear();

		if (isRecoveryJoin)
		{
			serverTickTimeline_.Clear();
			shouldReanchorNextPlayerSnapshot_ = true;
		}

		return true;
	}

	void ClientWorld::SetCurrentRoomId(RoomId roomId)
	{
		std::scoped_lock lock(worldMutex_);

		currentRoomId_ = roomId;
	}

	void ClientWorld::SetInterpolationEnabled(bool isEnabled) noexcept
	{
		std::scoped_lock lock(worldMutex_);

		interpolationEnabled_ = isEnabled;
	}

	bool ClientWorld::ToggleInterpolationEnabled() noexcept
	{
		std::scoped_lock lock(worldMutex_);

		interpolationEnabled_ = !interpolationEnabled_;
		return interpolationEnabled_;
	}

	void ClientWorld::SetInterpolationDelay(common::time::Milliseconds interpolationDelay) noexcept
	{
		std::scoped_lock lock(worldMutex_);

		interpolationDelayController_.SetDelay(interpolationDelay);
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

	ClientWorld::RenderPlayerStateList ClientWorld::GetRenderPlayerStatesSnapshot(common::time::TimePoint renderTime) const
	{
		std::scoped_lock lock(worldMutex_);

		return BuildRenderPlayerStateList(renderTime);
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

	bool ClientWorld::IsInterpolationEnabled() const noexcept
	{
		std::scoped_lock lock(worldMutex_);

		return interpolationEnabled_;
	}

	common::time::Milliseconds ClientWorld::GetInterpolationDelay() const noexcept
	{
		std::scoped_lock lock(worldMutex_);

		return interpolationDelayController_.GetDelay();
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