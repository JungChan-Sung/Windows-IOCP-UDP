#pragma once

namespace client::render
{
	inline constexpr int gridSize = 50;

	inline constexpr int playerOutlineThickness = 2;
	inline constexpr int invincibleOutlineThickness = 3;

	inline constexpr int bulletHalfSize = 2;

	inline constexpr int playerLabelOffsetY = 18;
	inline constexpr int statusLabelOffsetY = 34;

	inline constexpr int hudLeft = 16;
	inline constexpr int hudTop = 16;
	inline constexpr int hudLineHeight = 20;

	inline constexpr int screenEdgeFlashThickness = 16;

	inline constexpr float deadPlayerAlpha = 0.45F;
	inline constexpr float blinkMinAlpha = 0.35F;

	inline constexpr float impactBaseRadius = 6.0F;
	inline constexpr float impactMaxRadius = 18.0F;

	inline constexpr float spawnBaseRadius = 10.0F;
	inline constexpr float spawnMaxRadius = 28.0F;

	inline constexpr float spawnEffectMatchRadius = 36.0F;
	inline constexpr float spawnEffectMatchRadiusSquared = spawnEffectMatchRadius * spawnEffectMatchRadius;

	inline constexpr int scoreboardStartX = hudLeft;
	inline constexpr int scoreboardStartY = hudTop + (hudLineHeight * 9);
	inline constexpr int scoreboardLineHeight = 20;
}