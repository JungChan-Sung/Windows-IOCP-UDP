#pragma once

#include <cstdint>

namespace common::game
{
	enum class WeaponType : std::uint8_t
	{
		Basic = 0
	};

	inline constexpr int defaultBasicBulletDamage = 1;
	inline constexpr float defaultBasicBulletSpeed = 600.0F;
	inline constexpr float defaultBasicBulletLifeSeconds = 1.5F;
	inline constexpr float defaultBasicBulletRadius = 6.0F;
	inline constexpr float defaultBasicFireCooldownSeconds = 0.15F;

	// Bullet 생성과 발사 제한에 필요한 무기별 Gameplay 값을 정의하는 구조체
	struct WeaponRule
	{
	public:
		int bulletDamage = defaultBasicBulletDamage;
		float bulletSpeed = defaultBasicBulletSpeed;
		float bulletLifeSeconds = defaultBasicBulletLifeSeconds;
		float bulletRadius = defaultBasicBulletRadius;
		float fireCooldownSeconds = defaultBasicFireCooldownSeconds;
	};

	inline constexpr WeaponRule defaultBasicWeaponRule{};

	struct WeaponRuleConfig
	{
	public:
		WeaponRule basicWeaponRule = defaultBasicWeaponRule;
	};

	[[nodiscard]] constexpr const WeaponRule& GetWeaponRule(WeaponType weaponType, const WeaponRuleConfig& weaponRuleConfig) noexcept
	{
		switch (weaponType)
		{
		case WeaponType::Basic:
			return weaponRuleConfig.basicWeaponRule;

		default:
			return weaponRuleConfig.basicWeaponRule;
		}
	}

	[[nodiscard]] constexpr const WeaponRule& GetWeaponRule(WeaponType weaponType) noexcept
	{
		switch (weaponType)
		{
		case WeaponType::Basic:
			return defaultBasicWeaponRule;

		default:
			return defaultBasicWeaponRule;
		}
	}
}