#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include <Common/Identity/IdentityTypes.h>
#include <Common/Net/Endpoint.h>
#include <Common/Net/SessionToken.h>
#include <Common/Time/TimeTypes.h>

namespace server::net
{
	struct AuthenticatedAccount
	{
	public:
		common::identity::AccountId accountId = 0;
		common::identity::PersistentPlayerId persistentPlayerId = 0;
		common::net::SessionToken sessionToken{};
		std::string nickname;
		common::time::TimePoint authenticatedTime{};
	};

	class AuthenticatedAccountRegistry final
	{
	public:
		using EndpointKey = common::net::EndpointKey;
		using TimePoint = common::time::TimePoint;
		using Duration = common::time::Duration;

	private:
		using AccountTable = std::unordered_map<EndpointKey, AuthenticatedAccount, common::net::EndpointKeyHasher>;

	private:
		AccountTable accountTable_;

	public:
		AuthenticatedAccountRegistry() = default;
		~AuthenticatedAccountRegistry() noexcept = default;

		AuthenticatedAccountRegistry(const AuthenticatedAccountRegistry&) = delete;
		AuthenticatedAccountRegistry& operator=(const AuthenticatedAccountRegistry&) = delete;

		AuthenticatedAccountRegistry(AuthenticatedAccountRegistry&&) = delete;
		AuthenticatedAccountRegistry& operator=(AuthenticatedAccountRegistry&&) = delete;

	public:
		[[nodiscard]] bool Upsert(
			const EndpointKey& endpointKey,
			std::int64_t accountId,
			std::int64_t persistentPlayerId,
			common::net::SessionToken sessionToken,
			std::string nickname,
			TimePoint authenticatedTime
		);

		[[nodiscard]] AuthenticatedAccount* Find(const EndpointKey& endpointKey) noexcept;
		[[nodiscard]] const AuthenticatedAccount* Find(const EndpointKey& endpointKey) const noexcept;
		[[nodiscard]] AuthenticatedAccount* Find(const EndpointKey& endpointKey, common::net::SessionToken sessionToken) noexcept;
		[[nodiscard]] const AuthenticatedAccount* Find(const EndpointKey& endpointKey, common::net::SessionToken sessionToken) const noexcept;
		[[nodiscard]] std::optional<EndpointKey> FindEndpointByAccountId(std::int64_t accountId) const noexcept;

		[[nodiscard]] bool Remove(const EndpointKey& endpointKey) noexcept;
		[[nodiscard]] std::size_t RemoveExpired(TimePoint currentTime, Duration timeout) noexcept;

		void Clear() noexcept;

	public:
		[[nodiscard]] bool Contains(const EndpointKey& endpointKey) const noexcept;

		[[nodiscard]] std::size_t GetCount() const noexcept;
	};
}