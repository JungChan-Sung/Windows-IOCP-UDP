#include "LocalPlayerReconciliation.h"

#include <algorithm>
#include <cmath>

#include <Common/Net/SequenceNumber.h>

namespace client::game
{

	float LocalPlayerReconciliation::LengthSquared(float x, float y) noexcept
	{
		return (x * x) + (y * y);
	}

	float LocalPlayerReconciliation::Lerp(float startValue, float endValue, float alpha) noexcept
	{
		return startValue + ((endValue - startValue) * alpha);
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
		prediction_.Clear();

		renderCorrectionOffsetX_ = 0.0F;
		renderCorrectionOffsetY_ = 0.0F;
	}

	void LocalPlayerReconciliation::Reset(float x, float y) noexcept
	{
		pendingInputList_.clear();

		prediction_.Reset(x, y);

		renderCorrectionOffsetX_ = 0.0F;
		renderCorrectionOffsetY_ = 0.0F;
	}

	void LocalPlayerReconciliation::ApplyPredictionTick(std::uint32_t inputSequence, common::game::InputFlags inputFlags, float deltaSeconds, float moveSpeed, RoomId roomId)
	{
		if (!prediction_.IsInitialized() || deltaSeconds <= 0.0F || moveSpeed <= 0.0F)
		{
			return;
		}

		pendingInputList_.push_back(PendingInput{
			.sequence = inputSequence,
			.inputFlags = inputFlags,
			.deltaSeconds = deltaSeconds,
			});

		prediction_.ApplyInput(inputFlags, deltaSeconds, moveSpeed, roomId);
	}

	void LocalPlayerReconciliation::Reconcile(float authoritativeX, float authoritativeY, std::uint32_t lastProcessedInputSequence, float moveSpeed, RoomId roomId) noexcept
	{
		while (!pendingInputList_.empty() && common::net::IsSequenceOlderOrEqual(pendingInputList_.front().sequence, lastProcessedInputSequence))
		{
			pendingInputList_.pop_front();
		}

		LocalPlayerPrediction replayPrediction;
		replayPrediction.Reset(authoritativeX, authoritativeY);

		for (const PendingInput& pendingInput : pendingInputList_)
		{
			replayPrediction.ApplyInput(pendingInput.inputFlags, pendingInput.deltaSeconds, moveSpeed, roomId);
		}

		const float reconciledX = replayPrediction.GetX();
		const float reconciledY = replayPrediction.GetY();

		if (!prediction_.IsInitialized())
		{
			prediction_.Reset(reconciledX, reconciledY);

			renderCorrectionOffsetX_ = 0.0F;
			renderCorrectionOffsetY_ = 0.0F;
			return;
		}

		const float oldPredictedX = prediction_.GetX();
		const float oldPredictedY = prediction_.GetY();
		const float correctionDeltaX = reconciledX - oldPredictedX;
		const float correctionDeltaY = reconciledY - oldPredictedY;

		const float correctionDistanceSquared = LengthSquared(correctionDeltaX, correctionDeltaY);
		const float ignoreDistanceSquared = correctionIgnoreDistance * correctionIgnoreDistance;
		if (correctionDistanceSquared <= ignoreDistanceSquared)
		{
			return;
		}

		prediction_.Reset(reconciledX, reconciledY);

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

		renderCorrectionOffsetX_ = Lerp(renderCorrectionOffsetX_, 0.0F, alpha);
		renderCorrectionOffsetY_ = Lerp(renderCorrectionOffsetY_, 0.0F, alpha);

		const float clearDistanceSquared = renderCorrectionClearDistance * renderCorrectionClearDistance;
		if (LengthSquared(renderCorrectionOffsetX_, renderCorrectionOffsetY_) <= clearDistanceSquared)
		{
			renderCorrectionOffsetX_ = 0.0F;
			renderCorrectionOffsetY_ = 0.0F;
		}
	}
}