#include "AuthenticatedAccountRegistryTests.h"

#include <chrono>
#include <string>

#include <Common/Net/Endpoint.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Net/AuthenticatedAccountRegistry.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using AuthenticatedAccountRegistry
		= server::net::AuthenticatedAccountRegistry;

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

	void RunInitialStateTest(
		tests::DebugTestResult& result
	)
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

	void RunInsertAndFindTest(
		tests::DebugTestResult& result
	)
	{
		AuthenticatedAccountRegistry registry;

		const common::net::EndpointKey endpointKey
			= MakeEndpointKey(1, 1000);

		const common::time::TimePoint authenticatedTime
			= common::time::TimePoint{}
		+ common::time::Seconds(10);

		const bool inserted = registry.Upsert(
			endpointKey,
			1001,
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

		const server::net::AuthenticatedAccount* account
			= registry.Find(endpointKey);

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
			account->nickname == "nickname",
			"AuthenticatedAccountRegistry: nickname preserved"
		);

		tests::Expect(
			result,
			account->authenticatedTime == authenticatedTime,
			"AuthenticatedAccountRegistry: authentication time preserved"
		);
	}

	void RunInvalidAccountIdTest(
		tests::DebugTestResult& result
	)
	{
		AuthenticatedAccountRegistry registry;

		const bool inserted = registry.Upsert(
			MakeEndpointKey(1, 1000),
			0,
			"nickname",
			common::time::TimePoint{}
		);

		tests::Expect(
			result,
			!inserted,
			"AuthenticatedAccountRegistry: invalid account id rejected"
		);

		tests::Expect(
			result,
			registry.GetCount() == 0,
			"AuthenticatedAccountRegistry: invalid insert does not change count"
		);

		const bool emptyNicknameInserted = registry.Upsert(
			MakeEndpointKey(2, 2000),
			1001,
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

	void RunReplaceTest(
		tests::DebugTestResult& result
	)
	{
		AuthenticatedAccountRegistry registry;

		const common::net::EndpointKey endpointKey
			= MakeEndpointKey(1, 1000);

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1001,
				"first",
				common::time::TimePoint{}
			)
			);

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1002,
				"second",
				common::time::TimePoint{}
				+ common::time::Seconds(1)
			)
			);

		tests::Expect(
			result,
			registry.GetCount() == 1,
			"AuthenticatedAccountRegistry: replacement keeps count"
		);

		const server::net::AuthenticatedAccount* account
			= registry.Find(endpointKey);

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
			account->nickname == "second",
			"AuthenticatedAccountRegistry: nickname replaced"
		);
	}

	void RunRemoveTest(
		tests::DebugTestResult& result
	)
	{
		AuthenticatedAccountRegistry registry;

		const common::net::EndpointKey endpointKey
			= MakeEndpointKey(1, 1000);

		static_cast<void>(
			registry.Upsert(
				endpointKey,
				1001,
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

	void RunRemoveExpiredTest(
		tests::DebugTestResult& result
	)
	{
		AuthenticatedAccountRegistry registry;

		const common::time::TimePoint baseTime{};

		static_cast<void>(
			registry.Upsert(
				MakeEndpointKey(1, 1000),
				1001,
				"expired",
				baseTime
			)
			);

		static_cast<void>(
			registry.Upsert(
				MakeEndpointKey(2, 2000),
				1002,
				"active",
				baseTime + common::time::Seconds(8)
			)
			);

		const std::size_t removedCount
			= registry.RemoveExpired(
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

	void RunClearTest(
		tests::DebugTestResult& result
	)
	{
		AuthenticatedAccountRegistry registry;

		static_cast<void>(
			registry.Upsert(
				MakeEndpointKey(1, 1000),
				1001,
				"first",
				common::time::TimePoint{}
			)
			);

		static_cast<void>(
			registry.Upsert(
				MakeEndpointKey(2, 2000),
				1002,
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
		RunInvalidAccountIdTest(result);
		RunReplaceTest(result);
		RunRemoveTest(result);
		RunRemoveExpiredTest(result);
		RunClearTest(result);

		return result;
	}
}