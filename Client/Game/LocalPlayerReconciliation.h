#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

#include <Common/Game/GameTypes.h>
#include <Common/Game/InputFlags.h>

#include <Client/Game/LocalPlayerPrediction.h>

namespace client::game
{
	class LocalPlayerReconciliation
	{
	public:
		using RoomId = common::game::RoomId;

	public:
		struct PendingInput
		{
			std::uint32_t sequence = 0;
			common::game::InputFlags inputFlags{};
			float deltaSeconds = 0.0F;
		};

	public:
		using PendingInputList = std::deque<PendingInput>;

	public:
		LocalPlayerReconciliation() = default;
		~LocalPlayerReconciliation() noexcept = default;

		LocalPlayerReconciliation(const LocalPlayerReconciliation&) = delete;
		LocalPlayerReconciliation& operator=(const LocalPlayerReconciliation&) = delete;

		LocalPlayerReconciliation(LocalPlayerReconciliation&&) = delete;
		LocalPlayerReconciliation& operator=(LocalPlayerReconciliation&&) = delete;

	private:
		static inline constexpr float correctionIgnoreDistance = 2.0F;
		static inline constexpr float correctionHardSnapDistance = 160.0F;
		static inline constexpr float renderCorrectionMaxOffset = 96.0F;
		static inline constexpr float renderCorrectionSmoothSpeed = 12.0F;
		static inline constexpr float renderCorrectionClearDistance = 0.25F;

	private:
		PendingInputList pendingInputList_;
		LocalPlayerPrediction prediction_;

		float renderCorrectionOffsetX_ = 0.0F;
		float renderCorrectionOffsetY_ = 0.0F;

	private:
		[[nodiscard]] static float LengthSquared(float x, float y) noexcept;
		[[nodiscard]] static float Lerp(float startValue, float endValue, float alpha) noexcept;
		static void ClampVectorLength(float& x, float& y, float maxLength) noexcept;

	public:
		void Clear() noexcept;
		void Reset(float x, float y) noexcept;

		void ApplyPredictionTick(
			std::uint32_t inputSequence,
			common::game::InputFlags inputFlags,
			float deltaSeconds,
			float moveSpeed,
			RoomId roomId
		);
		void Reconcile(
			float authoritativeX,
			float authoritativeY,
			std::uint32_t lastProcessedInputSequence,
			float moveSpeed,
			RoomId roomId
		) noexcept;

		void UpdateRenderCorrection(float deltaSeconds) noexcept;

	public:
		[[nodiscard]] bool IsInitialized() const noexcept
		{
			return prediction_.IsInitialized();
		}

		[[nodiscard]] float GetPredictedX() const noexcept
		{
			return prediction_.GetX();
		}

		[[nodiscard]] float GetPredictedY() const noexcept
		{
			return prediction_.GetY();
		}

		[[nodiscard]] float GetRenderX() const noexcept
		{
			return prediction_.GetX() + renderCorrectionOffsetX_;
		}

		[[nodiscard]] float GetRenderY() const noexcept
		{
			return prediction_.GetY() + renderCorrectionOffsetY_;
		}

		[[nodiscard]] std::size_t GetPendingInputCount() const noexcept
		{
			return pendingInputList_.size();
		}
	};
}