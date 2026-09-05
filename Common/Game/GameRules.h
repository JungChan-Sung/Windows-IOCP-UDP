#pragma once

namespace common::game
{
	inline constexpr int defaultInitialPlayerHp = 3;

	inline constexpr float defaultRespawnDelaySeconds = 3.0F;
	inline constexpr float defaultRespawnInvincibilitySeconds = 1.2F;
	inline constexpr float defaultHitFlashDurationSeconds = 0.15F;

	// 플레이어 생존·리스폰·피격 상태에 적용되는 게임 규칙을 정의하는 구조체
	struct GameRuleConfig
	{
	public:
		int initialPlayerHp = defaultInitialPlayerHp;
		float respawnDelaySeconds = defaultRespawnDelaySeconds;
		float respawnInvincibilitySeconds = defaultRespawnInvincibilitySeconds;
		float hitFlashDurationSeconds = defaultHitFlashDurationSeconds;
	};
}