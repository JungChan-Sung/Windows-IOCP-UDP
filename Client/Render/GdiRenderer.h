#pragma once

#include <Windows.h>

#include <cstdint>
#include <vector>

#include <Common/Game/GameTypes.h>

#include <Client/Game/ClientWorld.h>

namespace client::render
{
	class GdiRenderer
	{
	public:
		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;

	public:
		GdiRenderer() = default;
		~GdiRenderer() noexcept = default;

		GdiRenderer(const GdiRenderer&) = delete;
		GdiRenderer& operator=(const GdiRenderer&) = delete;

		GdiRenderer(GdiRenderer&&) = delete;
		GdiRenderer& operator=(GdiRenderer&&) = delete;

	public:
		void Render(
			HDC deviceContext,
			const RECT& clientRect,
			const game::ClientWorld::RenderPlayerStateList& renderPlayerStateList,
			const game::ClientWorld::RenderBulletStateList& renderBulletStateList,
			const game::ClientWorld::RenderImpactEffectStateList& renderImpactEffectStateList,
			PlayerId localPlayerId,
			RoomId currentRoomId,
			std::uint32_t serverTick,
			int interpolationDelayMs
		) const;

	private:
		void DrawBackground(HDC deviceContext, const RECT& clientRect) const;
		void DrawGrid(HDC deviceContext, const RECT& clientRect) const;
		void DrawPlayers(
			HDC deviceContext,
			const game::ClientWorld::RenderPlayerStateList& renderPlayerStateList,
			const game::ClientWorld::RenderImpactEffectStateList& renderImpactEffectStateList,
			PlayerId localPlayerId
		) const;
		void DrawBullets(
			HDC deviceContext,
			const game::ClientWorld::RenderBulletStateList& renderBulletStateList
		) const;
		void DrawImpactEffects(
			HDC deviceContext,
			const game::ClientWorld::RenderImpactEffectStateList& renderImpactEffectStateList
		) const;
		void DrawHud(
			HDC deviceContext,
			const RECT& clientRect,
			const game::ClientWorld::RenderPlayerStateList& renderPlayerStateList,
			const game::ClientWorld::RenderImpactEffectStateList& renderImpactEffectStateList,
			PlayerId localPlayerId,
			RoomId currentRoomId,
			std::uint32_t serverTick,
			int interpolationDelayMs
		) const;
		void DrawWalls(HDC deviceContext, RoomId roomId) const;
		void DrawScreenEdgeFlash(
			HDC deviceContext,
			const RECT& clientRect,
			const game::ClientWorld::RenderPlayerStateList& renderPlayerStateList,
			const game::ClientWorld::RenderImpactEffectStateList& renderImpactEffectStateList,
			PlayerId localPlayerId
		) const;
		void DrawScoreboard(
			HDC deviceContext,
			const game::ClientWorld::RenderPlayerStateList& renderPlayerStateList,
			PlayerId localPlayerId
		) const;

	private:
		[[nodiscard]] RECT GetPlayerRect(float x, float y) const noexcept;
	};
}

