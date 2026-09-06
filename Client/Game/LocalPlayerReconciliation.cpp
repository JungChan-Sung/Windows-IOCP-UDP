#include "LocalPlayerReconciliation.h"

#include <algorithm>
#include <cmath>

#include <Common/Net/SequenceNumber.h>

#include <Client/Game/LocalPlayerPrediction.h>

namespace client::game
{

	float LocalPlayerReconciliation::LengthSquared(float x, float y) noexcept
	{
		return (x * x) + (y * y);
	}

	void LocalPlayerReconciliation::ClampVectorLength(float& x, float& y, float maxLength) noexcept
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

	void LocalPlayerReconciliation::Clear() noexcept
	{
		pendingInputList_.clear();

		ClearRenderCorrection();

		isHistoryValid_ = true;
	}

	void LocalPlayerReconciliation::ClearRenderCorrection() noexcept
	{
		renderCorrectionOffsetX_ = 0.0F;
		renderCorrectionOffsetY_ = 0.0F;
	}

	void LocalPlayerReconciliation::RecordPendingInput(std::uint32_t inputSequence, common::game::InputFlags inputFlags, float deltaSeconds)
	{
		if (deltaSeconds <= 0.0F || !isHistoryValid_)
		{
			return;
		}

		if (pendingInputList_.size() >= maxPendingInputCount)
		{
			pendingInputList_.clear();
			isHistoryValid_ = false;
			return;
		}

		PendingInput pendingInput{};
		pendingInput.sequence = inputSequence;
		pendingInput.inputFlags = inputFlags;
		pendingInput.deltaSeconds = deltaSeconds;

		pendingInputList_.push_back(pendingInput);
	}

	void LocalPlayerReconciliation::DiscardProcessedInputs(std::uint32_t lastProcessedInputSequence) noexcept
	{
		while (!pendingInputList_.empty() && common::net::IsSequenceOlderOrEqual(pendingInputList_.front().sequence, lastProcessedInputSequence))
		{
			pendingInputList_.pop_front();
		}
	}

	void LocalPlayerReconciliation::Reconcile(LocalPlayerPrediction& prediction, float authoritativeX, float authoritativeY, std::uint32_t lastProcessedInputSequence, float moveSpeed, RoomId roomId) noexcept
	{
		if (!isHistoryValid_)
		{
			pendingInputList_.clear();

			prediction.Reset(authoritativeX, authoritativeY);

			renderCorrectionOffsetX_ = 0.0F;
			renderCorrectionOffsetY_ = 0.0F;

			isHistoryValid_ = true;
			return;
		}

		DiscardProcessedInputs(lastProcessedInputSequence);

		LocalPlayerPrediction replayPrediction;
		replayPrediction.Reset(authoritativeX, authoritativeY);

		for (const PendingInput& pendingInput : pendingInputList_)
		{
			replayPrediction.ApplyInput(pendingInput.inputFlags, pendingInput.deltaSeconds, moveSpeed, roomId);
		}

		const float reconciledX = replayPrediction.GetX();
		const float reconciledY = replayPrediction.GetY();

		if (!prediction.IsInitialized())
		{
			prediction.Reset(reconciledX, reconciledY);

			renderCorrectionOffsetX_ = 0.0F;
			renderCorrectionOffsetY_ = 0.0F;
			return;
		}

		const float oldPredictedX = prediction.GetX();
		const float oldPredictedY = prediction.GetY();
		const float correctionDeltaX = reconciledX - oldPredictedX;
		const float correctionDeltaY = reconciledY - oldPredictedY;

		const float correctionDistanceSquared = LengthSquared(correctionDeltaX, correctionDeltaY);
		const float ignoreDistanceSquared = correctionIgnoreDistance * correctionIgnoreDistance;
		if (correctionDistanceSquared <= ignoreDistanceSquared)
		{
			return;
		}

		prediction.Reset(reconciledX, reconciledY);

		const float hardSnapDistanceSquared = correctionHardSnapDistance * correctionHardSnapDistance;
		if (correctionDistanceSquared >= hardSnapDistanceSquared)
		{
			renderCorrectionOffsetX_ = 0.0F;
			renderCorrectionOffsetY_ = 0.0F;
			return;
		}

		renderCorrectionOffsetX_ += oldPredictedX - reconciledX;

		renderCorrectionOffsetY_ += oldPredictedY - reconciledY;

		ClampVectorLength(renderCorrectionOffsetX_, renderCorrectionOffsetY_, renderCorrectionMaxOffset);
	}

	void LocalPlayerReconciliation::UpdateRenderCorrection(float deltaSeconds) noexcept
	{
		if (deltaSeconds <= 0.0F)
		{
			return;
		}

		const float distanceSquared = LengthSquared(renderCorrectionOffsetX_, renderCorrectionOffsetY_);
		if (distanceSquared <= 0.0F)
		{
			return;
		}

		const float alpha = std::clamp(renderCorrectionSmoothSpeed * deltaSeconds, 0.0F, 1.0F);

		renderCorrectionOffsetX_ = std::lerp(renderCorrectionOffsetX_, 0.0F, alpha);
		renderCorrectionOffsetY_ = std::lerp(renderCorrectionOffsetY_, 0.0F, alpha);

		const float clearDistanceSquared = renderCorrectionClearDistance * renderCorrectionClearDistance;
		if (LengthSquared(renderCorrectionOffsetX_, renderCorrectionOffsetY_) <= clearDistanceSquared)
		{
			renderCorrectionOffsetX_ = 0.0F;
			renderCorrectionOffsetY_ = 0.0F;
		}
	}
}