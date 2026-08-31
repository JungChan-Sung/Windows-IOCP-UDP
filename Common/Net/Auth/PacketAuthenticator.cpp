#include "PacketAuthenticator.h"

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
	inline constexpr std::size_t sessionTokenKeySize = sizeof(std::uint64_t) * 2;

	using SessionTokenKey = std::array<std::uint8_t, sessionTokenKeySize>;

	void WriteUInt64LittleEndian(SessionTokenKey& key, std::size_t offset, std::uint64_t value) noexcept
	{
		for (std::size_t index = 0; index < sizeof(value); ++index)
		{
			key[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
		}
	}

	[[nodiscard]] SessionTokenKey BuildSessionTokenKey(const common::net::SessionToken& sessionToken) noexcept
	{
		SessionTokenKey key{};

		WriteUInt64LittleEndian(key, 0, sessionToken.high);
		WriteUInt64LittleEndian(key, sizeof(sessionToken.high), sessionToken.low);

		return key;
	}
}

namespace common::net
{
	bool ComputePacketAuthenticationTag(const SessionToken& sessionToken, std::span<const char> data, PacketAuthenticationTag& tag) noexcept
	{
		if (!IsValidSessionToken(sessionToken))
		{
			return false;
		}

		if (data.size() > std::numeric_limits<ULONG>::max())
		{
			return false;
		}

		const SessionTokenKey key = BuildSessionTokenKey(sessionToken);

		PUCHAR inputData = nullptr;

		if (!data.empty())
		{
			inputData = reinterpret_cast<PUCHAR>(const_cast<char*>(data.data()));
		}

		const NTSTATUS status = ::BCryptHash(
			BCRYPT_HMAC_SHA256_ALG_HANDLE,
			const_cast<PUCHAR>(key.data()),
			static_cast<ULONG>(key.size()),
			inputData,
			static_cast<ULONG>(data.size()),
			tag.data(),
			static_cast<ULONG>(tag.size())
		);

		return BCRYPT_SUCCESS(status);
	}

	bool VerifyPacketAuthenticationTag(const SessionToken& sessionToken, std::span<const char> data, const PacketAuthenticationTag& tag) noexcept
	{
		PacketAuthenticationTag expectedTag{};
		if (!ComputePacketAuthenticationTag(sessionToken, data, expectedTag))
		{
			return false;
		}

		std::uint8_t difference = 0;
		for (std::size_t index = 0; index < tag.size(); ++index)
		{
			difference |= static_cast<std::uint8_t>(expectedTag[index] ^ tag[index]);
		}

		return difference == 0;
	}
}