#pragma once

#include <Client/Config/ClientConfigDefaults.h>

namespace client::game
{
	inline constexpr auto defaultInterpolationDelay = config::defaultInterpolationDelay;
	inline constexpr auto minInterpolationDelay = config::minInterpolationDelay;
	inline constexpr auto maxInterpolationDelay = config::maxInterpolationDelay;

	inline constexpr auto defaultSnapshotAssemblyTimeout = config::defaultSnapshotAssemblyTimeout;
}