#include "SessionTokenGenerator.h"

#include <Windows.h>
#include <bcrypt.h>

#include <cstddef>
#include <type_traits>

namespace server::service
{
	std::optional<common::net::SessionToken> GenerateSessionToken() noexcept
	{
		static_assert(std::is_trivially_copyable_v<common::net::SessionToken>);
		static_assert(sizeof(common::net::SessionToken) == sizeof(std::uint64_t) * 2);

		for (std::size_t attempt = 0; attempt < 4; ++attempt)
		{
			common::net::SessionToken sessionToken{};

			const NTSTATUS status = ::BCryptGenRandom(
				nullptr,
				reinterpret_cast<PUCHAR>(&sessionToken),
				static_cast<ULONG>(sizeof(sessionToken)),
				BCRYPT_USE_SYSTEM_PREFERRED_RNG
			);
			if (!BCRYPT_SUCCESS(status))
			{
				return std::nullopt;
			}

			if (common::net::IsValidSessionToken(sessionToken))
			{
				return sessionToken;
			}
		}

		return std::nullopt;
	}
}