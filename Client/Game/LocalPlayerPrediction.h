#pragma once

#include <Common/Game/GameTypes.h>
#include <Common/Game/InputFlags.h>

namespace client::game
{
	class LocalPlayerPrediction
	{
	public:
		using RoomId = common::game::RoomId;

	private:
		float x_ = 0.0F;
		float y_ = 0.0F;
		bool isInitialized_ = false;

	public:
		LocalPlayerPrediction() = default;
		~LocalPlayerPrediction() noexcept = default;

		LocalPlayerPrediction(const LocalPlayerPrediction&) = delete;
		LocalPlayerPrediction& operator=(const LocalPlayerPrediction&) = delete;

		LocalPlayerPrediction(LocalPlayerPrediction&&) = delete;
		LocalPlayerPrediction& operator=(LocalPlayerPrediction&&) = delete;

	public:
		void Clear() noexcept;
		void Reset(float x, float y) noexcept;
		void ApplyInput(common::game::InputFlags inputFlags, float deltaSeconds, float moveSpeed, RoomId roomId) noexcept;

	public:
		[[nodiscard]] bool IsInitialized() const noexcept
		{
			return isInitialized_;
		}

		[[nodiscard]] float GetX() const noexcept
		{
			return x_;
		}

		[[nodiscard]] float GetY() const noexcept
		{
			return y_;
		}
	};
}