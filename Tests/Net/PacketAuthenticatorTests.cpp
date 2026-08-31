#include "PacketAuthenticatorTests.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <Common/Net/Auth/PacketAuthenticator.h>
#include <Common/Net/SessionToken.h>

namespace
{
	[[nodiscard]] common::net::SessionToken MakeTestSessionToken() noexcept
	{
		return common::net::SessionToken{
			.high = 0x0706050403020100ULL,
			.low = 0x0F0E0D0C0B0A0908ULL,
		};
	}

	void RunKnownHmacTest(tests::DebugTestResult& result)
	{
		const common::net::SessionToken sessionToken =
			MakeTestSessionToken();

		constexpr std::string_view message = "packet-authentication";

		common::net::PacketAuthenticationTag tag{};

		const bool computed =
			common::net::ComputePacketAuthenticationTag(
				sessionToken,
				std::span<const char>(message.data(), message.size()),
				tag
			);

		tests::Expect(
			result,
			computed,
			"PacketAuthenticator: known HMAC computed"
		);

		if (!computed)
		{
			return;
		}

		constexpr common::net::PacketAuthenticationTag expectedTag{
			0x53, 0x21, 0x35, 0xB3,
			0x29, 0x85, 0x56, 0x16,
			0x03, 0xD9, 0xA0, 0x04,
			0x26, 0x5C, 0xC5, 0x96,
			0xAB, 0x80, 0x26, 0x0C,
			0x27, 0xFB, 0xEF, 0x06,
			0x53, 0xF8, 0x8B, 0xF1,
			0x85, 0x2C, 0x2B, 0x6C,
		};

		tests::Expect(
			result,
			tag == expectedTag,
			"PacketAuthenticator: known HMAC matches"
		);
	}

	void RunVerifyCorrectTagTest(tests::DebugTestResult& result)
	{
		const common::net::SessionToken sessionToken =
			MakeTestSessionToken();

		constexpr std::string_view message = "authenticated packet";

		common::net::PacketAuthenticationTag tag{};

		const bool computed =
			common::net::ComputePacketAuthenticationTag(
				sessionToken,
				std::span<const char>(message.data(), message.size()),
				tag
			);

		tests::Expect(
			result,
			computed,
			"PacketAuthenticator: verify base tag computed"
		);

		if (!computed)
		{
			return;
		}

		tests::Expect(
			result,
			common::net::VerifyPacketAuthenticationTag(
				sessionToken,
				std::span<const char>(message.data(), message.size()),
				tag
			),
			"PacketAuthenticator: correct tag accepted"
		);
	}

	void RunTamperedDataRejectedTest(tests::DebugTestResult& result)
	{
		const common::net::SessionToken sessionToken =
			MakeTestSessionToken();

		constexpr std::string_view originalMessage = "original";
		constexpr std::string_view tamperedMessage = "tampered";

		common::net::PacketAuthenticationTag tag{};

		static_cast<void>(
			common::net::ComputePacketAuthenticationTag(
				sessionToken,
				std::span<const char>(
					originalMessage.data(),
					originalMessage.size()
				),
				tag
			)
			);

		tests::Expect(
			result,
			!common::net::VerifyPacketAuthenticationTag(
				sessionToken,
				std::span<const char>(
					tamperedMessage.data(),
					tamperedMessage.size()
				),
				tag
			),
			"PacketAuthenticator: tampered data rejected"
		);
	}

	void RunWrongSessionTokenRejectedTest(tests::DebugTestResult& result)
	{
		const common::net::SessionToken sessionToken =
			MakeTestSessionToken();

		const common::net::SessionToken wrongSessionToken{
			.high = 1,
			.low = 2,
		};

		constexpr std::string_view message = "packet";

		common::net::PacketAuthenticationTag tag{};

		static_cast<void>(
			common::net::ComputePacketAuthenticationTag(
				sessionToken,
				std::span<const char>(message.data(), message.size()),
				tag
			)
			);

		tests::Expect(
			result,
			!common::net::VerifyPacketAuthenticationTag(
				wrongSessionToken,
				std::span<const char>(message.data(), message.size()),
				tag
			),
			"PacketAuthenticator: wrong session token rejected"
		);
	}

	void RunTamperedTagRejectedTest(tests::DebugTestResult& result)
	{
		const common::net::SessionToken sessionToken =
			MakeTestSessionToken();

		constexpr std::string_view message = "packet";

		common::net::PacketAuthenticationTag tag{};

		static_cast<void>(
			common::net::ComputePacketAuthenticationTag(
				sessionToken,
				std::span<const char>(message.data(), message.size()),
				tag
			)
			);

		tag[0] ^= 0x01;

		tests::Expect(
			result,
			!common::net::VerifyPacketAuthenticationTag(
				sessionToken,
				std::span<const char>(message.data(), message.size()),
				tag
			),
			"PacketAuthenticator: tampered tag rejected"
		);
	}

	void RunInvalidSessionTokenRejectedTest(tests::DebugTestResult& result)
	{
		constexpr std::string_view message = "packet";

		common::net::PacketAuthenticationTag tag{};

		tests::Expect(
			result,
			!common::net::ComputePacketAuthenticationTag(
				common::net::invalidSessionToken,
				std::span<const char>(message.data(), message.size()),
				tag
			),
			"PacketAuthenticator: invalid session token rejected"
		);
	}
}

namespace tests::net
{
	DebugTestResult RunPacketAuthenticatorTests()
	{
		DebugTestResult result{};

		RunKnownHmacTest(result);
		RunVerifyCorrectTagTest(result);
		RunTamperedDataRejectedTest(result);
		RunWrongSessionTokenRejectedTest(result);
		RunTamperedTagRejectedTest(result);
		RunInvalidSessionTokenRejectedTest(result);

		return result;
	}
}