#include "LocalPlayerPrediction.h"

#include <Common/Game/RoomLayout.h>
#include <Common/Game/SimulationConstants.h>
#include <Common/Game/WorldCollision.h>

namespace client::game
{
	void LocalPlayerPrediction::Clear() noexcept
	{
		x_ = 0.0F;
		y_ = 0.0F;
		isInitialized_ = false;
	}

	void LocalPlayerPrediction::Reset(float x, float y) noexcept
	{
		x_ = x;
		y_ = y;
		isInitialized_ = true;
	}

	void LocalPlayerPrediction::ApplyInput(common::game::InputFlags inputFlags, float deltaSeconds, float moveSpeed, RoomId roomId) noexcept
	{
		if (!isInitialized_ || deltaSeconds <= 0.0F || moveSpeed <= 0.0F)
		{
			return;
		}

		common::game::MovePlayerWithWallCollision(
			x_,
			y_,
			inputFlags,
			deltaSeconds,
			moveSpeed,
			common::game::playerHalfExtent,
			common::game::defaultWorldBounds,
			common::game::GetWallRectListForRoom(roomId)
		);
	}
}