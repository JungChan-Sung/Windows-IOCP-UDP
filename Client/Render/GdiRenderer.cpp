#include "GdiRenderer.h"

#include <string>
#include <sstream>
#include <algorithm>

#include <Common/Game/RoomLayout.h>
#include <Common/Game/WorldCollision.h>
#include <Common/Game/GameRules.h>

#include <Client/Game/EffectConfig.h>
#include <Client/Render/GdiRenderConstants.h>
#include <Client/Render/GdiObject.h>

namespace
{
	[[nodiscard]] float GetSpawnEffectAlphaAroundPlayer(
		const client::game::ClientWorld::RenderImpactEffectStateList& renderImpactEffectStateList,
		float x,
		float y
	) noexcept
	{
		float maxAlpha = 0.0F;

		for (const client::game::ClientWorld::RenderImpactEffectState& renderImpactEffectState : renderImpactEffectStateList)
		{
			if (renderImpactEffectState.effectType != common::game::EffectType::Spawn)
			{
				continue;
			}

			const float deltaX = renderImpactEffectState.x - x;
			const float deltaY = renderImpactEffectState.y - y;
			const float distanceSquared = (deltaX * deltaX) + (deltaY * deltaY);

			if (distanceSquared > client::render::spawnEffectMatchRadiusSquared)
			{
				continue;
			}

			const float totalDurationSeconds = client::game::GetEffectDurationSeconds(renderImpactEffectState.effectType);
			if (totalDurationSeconds <= 0.0F)
			{
				continue;
			}

			const float alpha = std::clamp(renderImpactEffectState.remainingSeconds / totalDurationSeconds, 0.0F, 1.0F);

			if (alpha > maxAlpha)
			{
				maxAlpha = alpha;
			}
		}

		return maxAlpha;
	}
}

namespace client::render
{
	void GdiRenderer::Render(HDC deviceContext, const RECT& clientRect, const game::ClientWorld::RenderFrameSnapshot& renderFrameSnapshot) const
	{
		const game::ClientWorld::RenderPlayerStateList& playerStateList = renderFrameSnapshot.playerStateList;
		const game::ClientWorld::RenderBulletStateList& bulletStateList = renderFrameSnapshot.bulletStateList;
		const game::ClientWorld::RenderImpactEffectStateList& impactEffectStateList = renderFrameSnapshot.impactEffectStateList;

		const PlayerId localPlayerId = renderFrameSnapshot.localPlayerId;
		const RoomId currentRoomId = renderFrameSnapshot.currentRoomId;
		const std::uint32_t serverTick = renderFrameSnapshot.lastServerTick;
		const bool interpolationEnabled = renderFrameSnapshot.interpolationEnabled;
		const bool predictionEnabled = renderFrameSnapshot.predictionEnabled;
		const bool reconciliationEnabled = renderFrameSnapshot.reconciliationEnabled;
		const int interpolationDelayMs = static_cast<int>(renderFrameSnapshot.interpolationDelay.count());

		DrawBackground(deviceContext, clientRect);
		DrawGrid(deviceContext, clientRect);
		DrawWalls(deviceContext, currentRoomId);
		DrawImpactEffects(deviceContext, impactEffectStateList);
		DrawPlayers(deviceContext, playerStateList, impactEffectStateList, localPlayerId);
		DrawBullets(deviceContext, bulletStateList);
		DrawScreenEdgeFlash(deviceContext, clientRect, playerStateList, impactEffectStateList, localPlayerId);
		DrawHud(
			deviceContext,
			clientRect,
			playerStateList,
			impactEffectStateList,
			localPlayerId,
			currentRoomId,
			serverTick,
			interpolationEnabled,
			predictionEnabled,
			reconciliationEnabled,
			interpolationDelayMs
		);
		DrawScoreboard(deviceContext, playerStateList, localPlayerId);
	}

	void GdiRenderer::DrawBackground(HDC deviceContext, const RECT& clientRect) const
	{
		GdiBrush backgroundBrush(::CreateSolidBrush(RGB(24, 24, 24)));
		if (!backgroundBrush.IsValid())
		{
			return;
		}

		::FillRect(deviceContext, &clientRect, static_cast<HBRUSH>(backgroundBrush.Get()));
	}

	void GdiRenderer::DrawGrid(HDC deviceContext, const RECT& clientRect) const
	{
		GdiPen gridPen(::CreatePen(PS_SOLID, 1, RGB(48, 48, 48)));
		if (!gridPen.IsValid())
		{
			return;
		}

		ScopedSelectObject selectedPen(deviceContext, ToGdiObject(gridPen.Get()));
		if (!selectedPen.IsSelected())
		{
			return;
		}

		for (int x = clientRect.left; x < clientRect.right; x += gridSize)
		{
			::MoveToEx(deviceContext, x, clientRect.top, nullptr);
			::LineTo(deviceContext, x, clientRect.bottom);
		}

		for (int y = clientRect.top; y < clientRect.bottom; y += gridSize)
		{
			::MoveToEx(deviceContext, clientRect.left, y, nullptr);
			::LineTo(deviceContext, clientRect.right, y);
		}
	}

	void GdiRenderer::DrawPlayers(HDC deviceContext, const game::ClientWorld::RenderPlayerStateList& renderPlayerStateList, const game::ClientWorld::RenderImpactEffectStateList& renderImpactEffectStateList, PlayerId localPlayerId) const
	{
		for (const game::ClientWorld::RenderPlayerState& renderPlayerState : renderPlayerStateList)
		{
			const bool isLocalPlayer = renderPlayerState.playerId == localPlayerId;
			const bool isInvincible = (!renderPlayerState.isDead && renderPlayerState.invincibilityRemainingSeconds > 0.0F);

			const float spawnEffectAlpha = GetSpawnEffectAlphaAroundPlayer(
				renderImpactEffectStateList,
				renderPlayerState.x,
				renderPlayerState.y
			);

			const bool hasSpawnEffect = spawnEffectAlpha > 0.0F;

			bool isBlinkVisible = true;
			if (isInvincible && !hasSpawnEffect)
			{
				const int blinkFrame = static_cast<int>(renderPlayerState.invincibilityRemainingSeconds * 10.0F);
				isBlinkVisible = (blinkFrame % 2) == 0;
			}

			const RECT playerRect = GetPlayerRect(renderPlayerState.x, renderPlayerState.y);

			COLORREF playerColor = RGB(220, 220, 220);

			if (renderPlayerState.isDead)
			{
				playerColor = RGB(110, 110, 110);
			}
			else if (hasSpawnEffect)
			{
				playerColor = isLocalPlayer ? RGB(170, 235, 255) : RGB(190, 245, 255);
			}
			else if (renderPlayerState.hitFlashRemainingSeconds > 0.0F)
			{
				playerColor = RGB(255, 90, 90);
			}
			else if (isLocalPlayer)
			{
				playerColor = RGB(80, 160, 255);
			}

			if (isBlinkVisible)
			{
				GdiBrush playerBrush(::CreateSolidBrush(playerColor));
				if (playerBrush.IsValid())
				{
					::FillRect(deviceContext, &playerRect, playerBrush.Get());
				}
			}

			if (isInvincible)
			{
				const COLORREF outlineColor = hasSpawnEffect ? RGB(80, 220, 255) : RGB(255, 220, 80);
				const int outlineWidth = hasSpawnEffect ? invincibleOutlineThickness : playerOutlineThickness;

				GdiPen outlinePen(::CreatePen(PS_SOLID, outlineWidth, outlineColor));
				if (!outlinePen.IsValid())
				{
					continue;
				}

				ScopedSelectObject selectedPen(deviceContext, ToGdiObject(outlinePen.Get()));
				ScopedSelectObject selectedBrush(deviceContext, ::GetStockObject(HOLLOW_BRUSH));
				if (!selectedPen.IsSelected() || !selectedBrush.IsSelected())
				{
					continue;
				}

				::Rectangle(
					deviceContext,
					playerRect.left,
					playerRect.top,
					playerRect.right,
					playerRect.bottom
				);

				if (hasSpawnEffect)
				{
					::Rectangle(
						deviceContext,
						playerRect.left - invincibleOutlineThickness,
						playerRect.top - invincibleOutlineThickness,
						playerRect.right + invincibleOutlineThickness,
						playerRect.bottom + invincibleOutlineThickness
					);
				}
			}

			std::wstring label
				= L"Id: " + std::to_wstring(renderPlayerState.playerId)
				+ L" HP: " + std::to_wstring(renderPlayerState.hp);

			if (renderPlayerState.isDead)
			{
				label += L" Dead";
			}
			else if (hasSpawnEffect)
			{
				label += L" Spawn";
			}
			else if (isInvincible)
			{
				label += L" Invincible";
			}

			::SetBkMode(deviceContext, TRANSPARENT);
			::SetTextColor(deviceContext, RGB(240, 240, 240));

			::TextOutW(
				deviceContext,
				playerRect.left,
				playerRect.top - playerLabelOffsetY,
				label.c_str(),
				static_cast<int>(label.size())
			);
		}
	}

	void GdiRenderer::DrawBullets(HDC deviceContext, const game::ClientWorld::RenderBulletStateList& renderBulletStateList) const
	{
		GdiBrush bulletBrush(::CreateSolidBrush(RGB(255, 200, 60)));
		if (!bulletBrush.IsValid())
		{
			return;
		}

		for (const game::ClientWorld::RenderBulletState& renderBulletState : renderBulletStateList)
		{
			const int centerX = static_cast<int>(renderBulletState.x);
			const int centerY = static_cast<int>(renderBulletState.y);

			RECT bulletRect{};
			bulletRect.left = centerX - bulletHalfSize;
			bulletRect.top = centerY - bulletHalfSize;
			bulletRect.right = centerX + bulletHalfSize;
			bulletRect.bottom = centerY + bulletHalfSize;

			::FillRect(deviceContext, &bulletRect, bulletBrush.Get());
		}
	}

	void GdiRenderer::DrawImpactEffects(HDC deviceContext, const game::ClientWorld::RenderImpactEffectStateList& renderImpactEffectStateList) const
	{

		for (const game::ClientWorld::RenderImpactEffectState& renderImpactEffectState : renderImpactEffectStateList)
		{
			const float totalDurationSeconds = game::GetEffectDurationSeconds(renderImpactEffectState.effectType);
			if (totalDurationSeconds <= 0.0F)
			{
				continue;
			}

			const float alpha = std::clamp(renderImpactEffectState.remainingSeconds / totalDurationSeconds, 0.0F, 1.0F);

			const int centerX = static_cast<int>(renderImpactEffectState.x);
			const int centerY = static_cast<int>(renderImpactEffectState.y);

			switch (renderImpactEffectState.effectType)
			{
			case common::game::EffectType::Impact:
			{
				GdiPen effectPen(::CreatePen(PS_SOLID, 2, RGB(255, 200, 80)));
				if (!effectPen.IsValid())
				{
					break;
				}

				ScopedSelectObject selectedPen(deviceContext, ToGdiObject(effectPen.Get()));
				ScopedSelectObject selectedBrush(deviceContext, ::GetStockObject(HOLLOW_BRUSH));
				if (!selectedPen.IsSelected() || !selectedBrush.IsSelected())
				{
					break;
				}

				const int radius = 4 + static_cast<int>((1.0F - alpha) * impactBaseRadius);

				::Ellipse(
					deviceContext,
					centerX - radius,
					centerY - radius,
					centerX + radius,
					centerY + radius
				);

				break;
			}

			case common::game::EffectType::Spawn:
			{
				GdiPen effectPen(::CreatePen(PS_SOLID, 2, RGB(80, 220, 255)));
				if (!effectPen.IsValid())
				{
					break;
				}

				ScopedSelectObject selectedPen(deviceContext, ToGdiObject(effectPen.Get()));
				ScopedSelectObject selectedBrush(deviceContext, ::GetStockObject(HOLLOW_BRUSH));
				if (!selectedPen.IsSelected() || !selectedBrush.IsSelected())
				{
					break;
				}

				const int outerRadius = static_cast<int>(spawnBaseRadius + ((1.0F - alpha) * (spawnMaxRadius - spawnBaseRadius)));
				const int innerRadius = std::max(1, outerRadius / 2);

				::Ellipse(
					deviceContext,
					centerX - outerRadius,
					centerY - outerRadius,
					centerX + outerRadius,
					centerY + outerRadius
				);

				::Ellipse(
					deviceContext,
					centerX - innerRadius,
					centerY - innerRadius,
					centerX + innerRadius,
					centerY + innerRadius
				);

				break;
			}

			default:
				break;
			}
		}
	}

	void GdiRenderer::DrawHud(HDC deviceContext, const RECT& clientRect, const game::ClientWorld::RenderPlayerStateList& renderPlayerStateList, const game::ClientWorld::RenderImpactEffectStateList& renderImpactEffectStateList, PlayerId localPlayerId, RoomId currentRoomId, std::uint32_t serverTick, bool interpolationEnabled, bool predictionEnabled, bool reconciliationEnabled, int interpolationDelayMs) const
	{
		(void)clientRect;

		::SetBkMode(deviceContext, TRANSPARENT);
		::SetTextColor(deviceContext, RGB(255, 255, 255));

		bool hasLocalPlayer = false;
		bool isLocalPlayerDead = false;
		bool isLocalPlayerInvincible = false;
		float localPlayerX = 0.0F;
		float localPlayerY = 0.0F;

		for (const game::ClientWorld::RenderPlayerState& renderPlayerState : renderPlayerStateList)
		{
			if (renderPlayerState.playerId != localPlayerId)
			{
				continue;
			}

			hasLocalPlayer = true;
			isLocalPlayerDead = renderPlayerState.isDead;
			isLocalPlayerInvincible = renderPlayerState.invincibilityRemainingSeconds > 0.0F;
			localPlayerX = renderPlayerState.x;
			localPlayerY = renderPlayerState.y;
			break;
		}

		std::wstring localState = L"Alive";

		if (hasLocalPlayer)
		{
			const float spawnEffectAlpha = GetSpawnEffectAlphaAroundPlayer(
				renderImpactEffectStateList,
				localPlayerX,
				localPlayerY
			);

			if (isLocalPlayerDead)
			{
				localState = L"Dead";
			}
			else if (spawnEffectAlpha > 0.0F)
			{
				localState = L"Spawn";
			}
			else if (isLocalPlayerInvincible)
			{
				localState = L"Invincible";
			}
		}

		const std::wstring line1 = L"LocalPlayerid: " + std::to_wstring(localPlayerId);
		const std::wstring line2 = L"CurrentRoomId: " + std::to_wstring(currentRoomId);
		const std::wstring line3 = L"ServerTick: " + std::to_wstring(serverTick);
		const std::wstring line4 = L"PlayerCount: " + std::to_wstring(renderPlayerStateList.size());
		const std::wstring line5 = interpolationEnabled ? L"RemoteInterpolation: ON" : L"RemoteInterpolation: OFF";
		const std::wstring line6 = predictionEnabled ? L"LocalPrediction: ON" : L"LocalPrediction: OFF";
		const std::wstring line7 = reconciliationEnabled ? L"Reconciliation: ON" : L"Reconciliation: OFF";
		const std::wstring line8 = L"InterpolationDelayMs: " + std::to_wstring(interpolationDelayMs);
		const std::wstring line9 = L"LocalState: " + localState;

		::TextOutW(
			deviceContext,
			hudLeft,
			hudTop + (hudLineHeight * 0),
			line1.c_str(),
			static_cast<int>(line1.size())
		);
		::TextOutW(
			deviceContext,
			hudLeft,
			hudTop + (hudLineHeight * 1),
			line2.c_str(),
			static_cast<int>(line2.size())
		);
		::TextOutW(
			deviceContext,
			hudLeft,
			hudTop + (hudLineHeight * 2),
			line3.c_str(),
			static_cast<int>(line3.size())
		);
		::TextOutW(
			deviceContext,
			hudLeft,
			hudTop + (hudLineHeight * 3),
			line4.c_str(),
			static_cast<int>(line4.size())
		);
		::TextOutW(
			deviceContext,
			hudLeft,
			hudTop + (hudLineHeight * 4),
			line5.c_str(),
			static_cast<int>(line5.size())
		);
		::TextOutW(
			deviceContext,
			hudLeft,
			hudTop + (hudLineHeight * 5),
			line6.c_str(),
			static_cast<int>(line6.size())
		);
		::TextOutW(
			deviceContext,
			hudLeft,
			hudTop + (hudLineHeight * 6),
			line7.c_str(),
			static_cast<int>(line7.size())
		);
		::TextOutW(
			deviceContext,
			hudLeft,
			hudTop + (hudLineHeight * 7),
			line8.c_str(),
			static_cast<int>(line8.size())
		);
		::TextOutW(
			deviceContext,
			hudLeft,
			hudTop + (hudLineHeight * 8),
			line9.c_str(),
			static_cast<int>(line9.size())
		);
	}

	void GdiRenderer::DrawWalls(HDC deviceContext, RoomId roomId) const
	{
		GdiBrush wallBrush(::CreateSolidBrush(RGB(90, 90, 90)));
		if (!wallBrush.IsValid())
		{
			return;
		}

		for (const common::game::WallRect& wallRect : common::game::GetWallRectListForRoom(roomId))
		{
			RECT rect{};
			rect.left = static_cast<LONG>(wallRect.minX);
			rect.top = static_cast<LONG>(wallRect.minY);
			rect.right = static_cast<LONG>(wallRect.maxX);
			rect.bottom = static_cast<LONG>(wallRect.maxY);

			::FillRect(deviceContext, &rect, wallBrush.Get());
		}
	}

	void GdiRenderer::DrawScreenEdgeFlash(HDC deviceContext, const RECT& clientRect, const game::ClientWorld::RenderPlayerStateList& renderPlayerStateList, const game::ClientWorld::RenderImpactEffectStateList& renderImpactEffectStateList, PlayerId localPlayerId) const
	{
		float localHitFlashRemainingSeconds = 0.0F;
		float localPlayerX = 0.0F;
		float localPlayerY = 0.0F;
		bool isLocalPlayerDead = false;
		bool hasLocalPlayer = false;

		for (const game::ClientWorld::RenderPlayerState& renderPlayerState : renderPlayerStateList)
		{
			if (renderPlayerState.playerId != localPlayerId)
			{
				continue;
			}

			localHitFlashRemainingSeconds = renderPlayerState.hitFlashRemainingSeconds;
			localPlayerX = renderPlayerState.x;
			localPlayerY = renderPlayerState.y;
			isLocalPlayerDead = renderPlayerState.isDead;
			hasLocalPlayer = true;
			break;
		}

		if (!hasLocalPlayer || isLocalPlayerDead || localHitFlashRemainingSeconds <= 0.0F)
		{
			return;
		}

		const float spawnEffectAlpha = GetSpawnEffectAlphaAroundPlayer(
			renderImpactEffectStateList,
			localPlayerX,
			localPlayerY
		);

		if (spawnEffectAlpha > 0.0F)
		{
			return;
		}

		const float normalizedIntensity = std::clamp(localHitFlashRemainingSeconds / common::game::defaultHitFlashDurationSeconds, 0.0F, 1.0F);

		const int outerThickness = screenEdgeFlashThickness + static_cast<int>(normalizedIntensity * screenEdgeFlashThickness);
		const int innerThickness = std::max(2, outerThickness / 2);

		const int outerRed = 80 + static_cast<int>(normalizedIntensity * 175.0F);
		const int innerRed = 40 + static_cast<int>(normalizedIntensity * 120.0F);

		GdiBrush outerBrush(::CreateSolidBrush(RGB(outerRed, 20, 20)));
		GdiBrush innerBrush(::CreateSolidBrush(RGB(innerRed, 10, 10)));
		if (!outerBrush.IsValid() || !innerBrush.IsValid())
		{
			return;
		}

		RECT topOuterRect{};
		topOuterRect.left = clientRect.left;
		topOuterRect.top = clientRect.top;
		topOuterRect.right = clientRect.right;
		topOuterRect.bottom = clientRect.top + outerThickness;

		RECT bottomOuterRect{};
		bottomOuterRect.left = clientRect.left;
		bottomOuterRect.top = clientRect.bottom - outerThickness;
		bottomOuterRect.right = clientRect.right;
		bottomOuterRect.bottom = clientRect.bottom;

		RECT leftOuterRect{};
		leftOuterRect.left = clientRect.left;
		leftOuterRect.top = clientRect.top;
		leftOuterRect.right = clientRect.left + outerThickness;
		leftOuterRect.bottom = clientRect.bottom;

		RECT rightOuterRect{};
		rightOuterRect.left = clientRect.right - outerThickness;
		rightOuterRect.top = clientRect.top;
		rightOuterRect.right = clientRect.right;
		rightOuterRect.bottom = clientRect.bottom;

		::FillRect(deviceContext, &topOuterRect, outerBrush.Get());
		::FillRect(deviceContext, &bottomOuterRect, outerBrush.Get());
		::FillRect(deviceContext, &leftOuterRect, outerBrush.Get());
		::FillRect(deviceContext, &rightOuterRect, outerBrush.Get());

		RECT topInnerRect{};
		topInnerRect.left = clientRect.left + outerThickness;
		topInnerRect.top = clientRect.top + outerThickness;
		topInnerRect.right = clientRect.right - outerThickness;
		topInnerRect.bottom = topInnerRect.top + innerThickness;

		RECT bottomInnerRect{};
		bottomInnerRect.left = clientRect.left + outerThickness;
		bottomInnerRect.top = clientRect.bottom - outerThickness - innerThickness;
		bottomInnerRect.right = clientRect.right - outerThickness;
		bottomInnerRect.bottom = clientRect.bottom - outerThickness;

		RECT leftInnerRect{};
		leftInnerRect.left = clientRect.left + outerThickness;
		leftInnerRect.top = clientRect.top + outerThickness;
		leftInnerRect.right = leftInnerRect.left + innerThickness;
		leftInnerRect.bottom = clientRect.bottom - outerThickness;

		RECT rightInnerRect{};
		rightInnerRect.left = clientRect.right - outerThickness - innerThickness;
		rightInnerRect.top = clientRect.top + outerThickness;
		rightInnerRect.right = clientRect.right - outerThickness;
		rightInnerRect.bottom = clientRect.bottom - outerThickness;

		::FillRect(deviceContext, &topInnerRect, innerBrush.Get());
		::FillRect(deviceContext, &bottomInnerRect, innerBrush.Get());
		::FillRect(deviceContext, &leftInnerRect, innerBrush.Get());
		::FillRect(deviceContext, &rightInnerRect, innerBrush.Get());
	}

	void GdiRenderer::DrawScoreboard(HDC deviceContext, const game::ClientWorld::RenderPlayerStateList& renderPlayerStateList, PlayerId localPlayerId) const
	{
		if (deviceContext == nullptr)
		{
			return;
		}

		std::wstring title = L"Scoreboard";
		::TextOutW(
			deviceContext,
			scoreboardStartX,
			scoreboardStartY,
			title.c_str(),
			static_cast<int>(title.size())
		);

		int currentY = scoreboardStartY + scoreboardLineHeight;

		for (const game::ClientWorld::RenderPlayerState& renderPlayerState : renderPlayerStateList)
		{
			std::wostringstream stream;

			if (renderPlayerState.playerId == localPlayerId)
			{
				stream << L"* ";
			}
			else
			{
				stream << L"  ";
			}

			stream << L"P" << renderPlayerState.playerId
				<< L"  K: " << renderPlayerState.killCount
				<< L"  D: " << renderPlayerState.deathCount
				<< L"  HP: " << renderPlayerState.hp;

			if (renderPlayerState.isDead)
			{
				stream << L"  DEAD";
			}

			const std::wstring text = stream.str();

			::TextOutW(
				deviceContext,
				scoreboardStartX,
				currentY,
				text.c_str(),
				static_cast<int>(text.size())
			);

			currentY += scoreboardLineHeight;
		}
	}

	RECT GdiRenderer::GetPlayerRect(float x, float y) const noexcept
	{
		const int halfSize = static_cast<int>(common::game::playerHalfExtent);
		const int centerX = static_cast<int>(x);
		const int centerY = static_cast<int>(y);

		RECT playerRect{};
		playerRect.left = centerX - halfSize;
		playerRect.top = centerY - halfSize;
		playerRect.right = centerX + halfSize;
		playerRect.bottom = centerY + halfSize;

		return playerRect;
	}
}