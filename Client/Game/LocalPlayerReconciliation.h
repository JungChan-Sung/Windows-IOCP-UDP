#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>

#include <Common/Game/GameTypes.h>
#include <Common/Game/InputFlags.h>

namespace client::game
{
	class LocalPlayerPrediction;

	class LocalPlayerReconciliation
	{
	private:
		struct PendingInput
		{
			std::uint32_t sequence = 0;
			common::game::InputFlags inputFlags = common::game::InputFlags::None;
			float deltaSeconds = 0.0F;
		};

	public:
		using RoomId = common::game::RoomId;

	private:
		using PendingInputList = std::deque<PendingInput>;

	private:
		static inline constexpr std::size_t maxPendingInputCount = 256;

		static inline constexpr float correctionIgnoreDistance = 2.0F;
		static inline constexpr float correctionHardSnapDistance = 160.0F;

		static inline constexpr float renderCorrectionMaxOffset = 96.0F;
		static inline constexpr float renderCorrectionSmoothSpeed = 12.0F;
		static inline constexpr float renderCorrectionClearDistance = 0.25F;

	private:
		PendingInputList pendingInputList_;

		float renderCorrectionOffsetX_ = 0.0F;
		float renderCorrectionOffsetY_ = 0.0F;

		bool isHistoryValid_ = true;

	public:
		LocalPlayerReconciliation() = default;
		~LocalPlayerReconciliation() noexcept = default;

		LocalPlayerReconciliation(const LocalPlayerReconciliation&) = delete;
		LocalPlayerReconciliation& operator=(const LocalPlayerReconciliation&) = delete;

		LocalPlayerReconciliation(LocalPlayerReconciliation&&) = delete;
		LocalPlayerReconciliation& operator=(LocalPlayerReconciliation&&) = delete;

	private:
		[[nodiscard]] static float LengthSquared(float x, float y) noexcept;
		[[nodiscard]] static float Lerp(float startValue, float endValue, float alpha) noexcept;

		static void ClampVectorLength(float& x, float& y, float maxLength) noexcept;

	public:
		void Clear() noexcept;
		void RecordPendingInput(std::uint32_t inputSequence, common::game::InputFlags inputFlags, float deltaSeconds);

		void Reconcile(
			LocalPlayerPrediction& prediction,
			float authoritativeX,
			float authoritativeY,
			std::uint32_t lastProcessedInputSequence,
			float moveSpeed,
			RoomId roomId
		) noexcept;

		void UpdateRenderCorrection(float deltaSeconds) noexcept;

	public:
		[[nodiscard]] float GetRenderCorrectionOffsetX() const noexcept
		{
			return renderCorrectionOffsetX_;
		}

		[[nodiscard]] float GetRenderCorrectionOffsetY() const noexcept
		{
			return renderCorrectionOffsetY_;
		}
	};
}