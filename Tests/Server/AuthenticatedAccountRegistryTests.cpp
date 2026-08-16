#include "AuthenticatedAccountRegistryTests.h"

#include <cstdint>
#include <optional>

#include <Common/Net/Endpoint.h>
#include <Common/Net/SessionToken.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Service/AuthenticatedAccountRegistry.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using AuthenticatedAccountRegistry =
		server::service::AuthenticatedAccountRegistry;

	[[nodiscard]] constexpr common::net::SessionToken MakeSessionToken(
		std::uint64_t value
	) noexcept
	{
		return common::net::SessionToken{
			.high = value,
			.low = value ^ 0xA5A5A5A5A5A5A5A5ULL,
		};
	}

	[[nodiscard]] common::net::EndpointKey MakeEndpointKey(
		std::uint32_t address,
		std::uint16_t port
	) noexcept
	{
		return common::net::EndpointKey{
			.address = address,
			.port = port,
		};
	}

	void RunInitialStateTest(tests::DebugTestResult& result)
	{
		const AuthenticatedAccountRegistry registry;

		tests::Expect(
			result,
			registry.GetCount() == 0,
			"AuthenticatedAccountRegistry: initially empty"
		);

		tests::Expect(
			result,
			!registry.Contains(MakeEndpointKey(1, 1000)),
			"AuthenticatedAccountRegistry: missing endpoint"
		);
	}

	void RunInsertAndFindTest(tests::DebugTestResult& result)
	{
		AuthenticatedAccountRegistry registry;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(1, 1000);

		const common::net::SessionToken sessionToken =
			MakeSessionToken(1001);

		const common::time::TimePoint authenticatedTime =
			common::time::TimePoint{} + common::time::Seconds(10);

		const bool inserted = registry.Upsert(
			endpointKey,
			1001,
			5001,
			sessionToken,
			"nickname",
			authenticatedTime
		);

		tests::Expect(
			result,
			inserted,
			"AuthenticatedAccountRegistry: valid account inserted"
		);

		tests::Expect(
			result,
			registry.GetCount() == 1,
			"AuthenticatedAccountRegistry: insert count"
		);

		const server::service::AuthenticatedAccount* account =
			registry.Find(endpointKey);

		tests::Expect(
			result,
			account != nullptr,
			"AuthenticatedAccountRegistry: inserted account found"
		);

		if (account == nullptr)
		{
			return;
		}

		tests::Expect(
			result,
			account->accountId == 1001,
			"AuthenticatedAccountRegistry: account id preserved"
		);

		tests::Expect(
			result,
			account->persistentPlayerId == 5001,
			"AuthenticatedAccountRegistry: persistent player id preserved"
		);

		tests::Expect(
			result,
			account->sessionToken == sessionToken,
			"AuthenticatedAccountRegistry: session token preserved"
		);

		tests::Expect(
			result,
			account->nickname == "nickname",
			"AuthenticatedAccountRegistry: nickname preserved"
		);

		tests::Expect(
			result,
			account->authenticatedTime == authenticatedTime,
			"AuthenticatedAccountRegistry: authentication time preserved"
		);
	}

	void RunFindBySessionTokenTest(tests::DebugTestResult& result)
	{
		AuthenticatedAccountRegistry registry;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(1, 1000);

		const common::net::SessionToken sessionToken =
			MakeSessionToken(1001);

		const common::net::SessionToken differentSessionToken =
			MakeSessionToken(9999);

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1001,
				5001,
				sessionToken,
				"nickname",
				common::time::TimePoint{}
			)
			);

		tests::Expect(
			result,
			registry.Find(endpointKey, sessionToken) != nullptr,
			"AuthenticatedAccountRegistry: matching session token found"
		);

		tests::Expect(
			result,
			registry.Find(endpointKey, differentSessionToken) == nullptr,
			"AuthenticatedAccountRegistry: mismatched session token rejected"
		);

		tests::Expect(
			result,
			registry.Find(endpointKey, common::net::invalidSessionToken) == nullptr,
			"AuthenticatedAccountRegistry: invalid session token rejected"
		);

		tests::Expect(
			result,
			registry.Find(
				MakeEndpointKey(2, 2000),
				sessionToken
			) == nullptr,
			"AuthenticatedAccountRegistry: unknown endpoint token rejected"
		);

		const AuthenticatedAccountRegistry& constRegistry = registry;

		tests::Expect(
			result,
			constRegistry.Find(endpointKey, sessionToken) != nullptr,
			"AuthenticatedAccountRegistry: const matching token found"
		);

		tests::Expect(
			result,
			constRegistry.Find(
				endpointKey,
				differentSessionToken
			) == nullptr,
			"AuthenticatedAccountRegistry: const mismatched token rejected"
		);
	}

	void RunFindEndpointByAccountIdTest(tests::DebugTestResult& result)
	{
		AuthenticatedAccountRegistry registry;

		const common::net::EndpointKey firstEndpointKey =
			MakeEndpointKey(1, 1000);

		const common::net::EndpointKey secondEndpointKey =
			MakeEndpointKey(2, 2000);

		static_cast<void>(
			registry.Upsert(
				firstEndpointKey,
				1001,
				5001,
				MakeSessionToken(1001),
				"first",
				common::time::TimePoint{}
			)
			);

		static_cast<void>(
			registry.Upsert(
				secondEndpointKey,
				1002,
				5002,
				MakeSessionToken(1002),
				"second",
				common::time::TimePoint{}
			)
			);

		const std::optional<common::net::EndpointKey>
			firstFoundEndpointKey =
			registry.FindEndpointByAccountId(1001);

		tests::Expect(
			result,
			firstFoundEndpointKey.has_value(),
			"AuthenticatedAccountRegistry: account endpoint found"
		);

		tests::Expect(
			result,
			firstFoundEndpointKey.has_value()
			&& *firstFoundEndpointKey == firstEndpointKey,
			"AuthenticatedAccountRegistry: correct account endpoint"
		);

		const std::optional<common::net::EndpointKey>
			secondFoundEndpointKey =
			registry.FindEndpointByAccountId(1002);

		tests::Expect(
			result,
			secondFoundEndpointKey.has_value()
			&& *secondFoundEndpointKey == secondEndpointKey,
			"AuthenticatedAccountRegistry: second account endpoint"
		);

		tests::Expect(
			result,
			!registry.FindEndpointByAccountId(9999).has_value(),
			"AuthenticatedAccountRegistry: unknown account endpoint missing"
		);

		tests::Expect(
			result,
			!registry.FindEndpointByAccountId(0).has_value(),
			"AuthenticatedAccountRegistry: invalid account endpoint missing"
		);
	}

	void RunInvalidAccountDataTest(tests::DebugTestResult& result)
	{
		AuthenticatedAccountRegistry registry;

		const bool invalidAccountIdInserted = registry.Upsert(
			MakeEndpointKey(1, 1000),
			0,
			5001,
			MakeSessionToken(1001),
			"nickname",
			common::time::TimePoint{}
		);

		tests::Expect(
			result,
			!invalidAccountIdInserted,
			"AuthenticatedAccountRegistry: invalid account id rejected"
		);

		const bool invalidPersistentPlayerIdInserted = registry.Upsert(
			MakeEndpointKey(2, 2000),
			1001,
			0,
			MakeSessionToken(1001),
			"nickname",
			common::time::TimePoint{}
		);

		tests::Expect(
			result,
			!invalidPersistentPlayerIdInserted,
			"AuthenticatedAccountRegistry: invalid persistent player id rejected"
		);

		const bool invalidSessionTokenInserted = registry.Upsert(
			MakeEndpointKey(3, 3000),
			1001,
			5001,
			common::net::invalidSessionToken,
			"nickname",
			common::time::TimePoint{}
		);

		tests::Expect(
			result,
			!invalidSessionTokenInserted,
			"AuthenticatedAccountRegistry: invalid session token rejected"
		);

		const bool emptyNicknameInserted = registry.Upsert(
			MakeEndpointKey(4, 4000),
			1001,
			5001,
			MakeSessionToken(1002),
			"",
			common::time::TimePoint{}
		);

		tests::Expect(
			result,
			!emptyNicknameInserted,
			"AuthenticatedAccountRegistry: empty nickname rejected"
		);

		tests::Expect(
			result,
			registry.GetCount() == 0,
			"AuthenticatedAccountRegistry: invalid accounts do not change count"
		);
	}

	void RunReplaceTest(tests::DebugTestResult& result)
	{
		AuthenticatedAccountRegistry registry;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(1, 1000);

		const common::net::SessionToken firstSessionToken =
			MakeSessionToken(1001);

		const common::net::SessionToken secondSessionToken =
			MakeSessionToken(1002);

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1001,
				5001,
				firstSessionToken,
				"first",
				common::time::TimePoint{}
			)
			);

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1002,
				5002,
				secondSessionToken,
				"second",
				common::time::TimePoint{} + common::time::Seconds(1)
			)
			);

		tests::Expect(
			result,
			registry.GetCount() == 1,
			"AuthenticatedAccountRegistry: replacement keeps count"
		);

		const server::service::AuthenticatedAccount* account =
			registry.Find(endpointKey);

		tests::Expect(
			result,
			account != nullptr,
			"AuthenticatedAccountRegistry: replacement account found"
		);

		if (account == nullptr)
		{
			return;
		}

		tests::Expect(
			result,
			account->accountId == 1002,
			"AuthenticatedAccountRegistry: account replaced"
		);

		tests::Expect(
			result,
			account->persistentPlayerId == 5002,
			"AuthenticatedAccountRegistry: persistent player id replaced"
		);

		tests::Expect(
			result,
			account->sessionToken == secondSessionToken,
			"AuthenticatedAccountRegistry: session token replaced"
		);

		tests::Expect(
			result,
			account->nickname == "second",
			"AuthenticatedAccountRegistry: nickname replaced"
		);
	}

	void RunRemoveTest(tests::DebugTestResult& result)
	{
		AuthenticatedAccountRegistry registry;

		const common::net::EndpointKey endpointKey =
			MakeEndpointKey(1, 1000);

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1001,
				5001,
				MakeSessionToken(1001),
				"nickname",
				common::time::TimePoint{}
			)
			);

		tests::Expect(
			result,
			registry.Remove(endpointKey),
			"AuthenticatedAccountRegistry: account removed"
		);

		tests::Expect(
			result,
			!registry.Remove(endpointKey),
			"AuthenticatedAccountRegistry: missing removal rejected"
		);

		tests::Expect(
			result,
			registry.GetCount() == 0,
			"AuthenticatedAccountRegistry: remove count"
		);
	}

	void RunRemoveExpiredTest(tests::DebugTestResult& result)
	{
		AuthenticatedAccountRegistry registry;

		const common::time::TimePoint baseTime{};

		static_cast<void>(
			registry.Upsert(
				MakeEndpointKey(1, 1000),
				1001,
				5001,
				MakeSessionToken(1001),
				"expired",
				baseTime
			)
			);

		static_cast<void>(
			registry.Upsert(
				MakeEndpointKey(2, 2000),
				1002,
				5002,
				MakeSessionToken(1002),
				"active",
				baseTime + common::time::Seconds(8)
			)
			);

		const std::size_t removedCount = registry.RemoveExpired(
			baseTime + common::time::Seconds(10),
			common::time::Seconds(5)
		);

		tests::Expect(
			result,
			removedCount == 1,
			"AuthenticatedAccountRegistry: one expired account removed"
		);

		tests::Expect(
			result,
			!registry.Contains(MakeEndpointKey(1, 1000)),
			"AuthenticatedAccountRegistry: expired account missing"
		);

		tests::Expect(
			result,
			registry.Contains(MakeEndpointKey(2, 2000)),
			"AuthenticatedAccountRegistry: active account retained"
		);
	}

	void RunClearTest(tests::DebugTestResult& result)
	{
		AuthenticatedAccountRegistry registry;

		static_cast<void>(
			registry.Upsert(
				MakeEndpointKey(1, 1000),
				1001,
				5001,
				MakeSessionToken(1001),
				"first",
				common::time::TimePoint{}
			)
			);

		static_cast<void>(
			registry.Upsert(
				MakeEndpointKey(2, 2000),
				1002,
				5002,
				MakeSessionToken(1002),
				"second",
				common::time::TimePoint{}
			)
			);

		registry.Clear();

		tests::Expect(
			result,
			registry.GetCount() == 0,
			"AuthenticatedAccountRegistry: clear"
		);
	}
}

namespace tests::server
{
	DebugTestResult RunAuthenticatedAccountRegistryTests()
	{
		DebugTestResult result{};

		RunInitialStateTest(result);
		RunInsertAndFindTest(result);
		RunFindBySessionTokenTest(result);
		RunFindEndpointByAccountIdTest(result);
		RunInvalidAccountDataTest(result);
		RunReplaceTest(result);
		RunRemoveTest(result);
		RunRemoveExpiredTest(result);
		RunClearTest(result);

		return result;
	}
}