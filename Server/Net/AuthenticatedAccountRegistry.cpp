#include "AuthenticatedAccountRegistry.h"

#include <utility>

namespace server::net
{
	bool AuthenticatedAccountRegistry::Upsert(const EndpointKey& endpointKey, std::int64_t accountId, std::int64_t persistentPlayerId, common::net::SessionToken sessionToken, std::string nickname, TimePoint authenticatedTime)
	{
		if (accountId <= 0 || !common::net::IsValidSessionToken(sessionToken) || nickname.empty())
		{
			return false;
		}

		accountTable_.insert_or_assign(
			endpointKey,
			AuthenticatedAccount{
				.accountId = accountId,
				.persistentPlayerId = persistentPlayerId,
				.sessionToken = sessionToken,
				.nickname = std::move(nickname),
				.authenticatedTime = authenticatedTime,
			}
			);

		return true;
	}

	AuthenticatedAccount* AuthenticatedAccountRegistry::Find(const EndpointKey& endpointKey) noexcept
	{
		const auto iterator = accountTable_.find(endpointKey);
		if (iterator == accountTable_.end())
		{
			return nullptr;
		}

		return &iterator->second;
	}

	const AuthenticatedAccount* AuthenticatedAccountRegistry::Find(const EndpointKey& endpointKey) const noexcept
	{
		const auto iterator = accountTable_.find(endpointKey);
		if (iterator == accountTable_.end())
		{
			return nullptr;
		}

		return &iterator->second;
	}

	AuthenticatedAccount* AuthenticatedAccountRegistry::Find(const EndpointKey& endpointKey, common::net::SessionToken sessionToken) noexcept
	{
		AuthenticatedAccount* account = Find(endpointKey);
		if (account == nullptr || account->sessionToken != sessionToken)
		{
			return nullptr;
		}

		return account;
	}

	const AuthenticatedAccount* AuthenticatedAccountRegistry::Find(const EndpointKey& endpointKey, common::net::SessionToken sessionToken) const noexcept
	{
		const AuthenticatedAccount* account = Find(endpointKey);
		if (account == nullptr || account->sessionToken != sessionToken)
		{
			return nullptr;
		}

		return account;
	}

	std::optional<AuthenticatedAccountRegistry::EndpointKey> AuthenticatedAccountRegistry::FindEndpointByAccountId(std::int64_t accountId) const noexcept
	{
		if (accountId <= 0)
		{
			return std::nullopt;
		}

		for (const auto& [endpointKey, account] : accountTable_)
		{
			if (account.accountId == accountId)
			{
				return endpointKey;
			}
		}

		return std::nullopt;
	}

	bool AuthenticatedAccountRegistry::Remove(const EndpointKey& endpointKey) noexcept
	{
		return accountTable_.erase(endpointKey) > 0;
	}

	std::size_t AuthenticatedAccountRegistry::RemoveExpired(TimePoint currentTime, Duration timeout) noexcept
	{
		if (timeout <= Duration::zero())
		{
			return 0;
		}

		std::size_t removedCount = 0;

		for (auto iterator = accountTable_.begin(); iterator != accountTable_.end();)
		{
			const AuthenticatedAccount& account = iterator->second;

			if (currentTime - account.authenticatedTime < timeout)
			{
				++iterator;
				continue;
			}

			iterator = accountTable_.erase(iterator);
			++removedCount;
		}

		return removedCount;
	}

	void AuthenticatedAccountRegistry::Clear() noexcept
	{
		accountTable_.clear();
	}

	bool AuthenticatedAccountRegistry::Contains(const EndpointKey& endpointKey) const noexcept
	{
		return accountTable_.contains(endpointKey);
	}

	std::size_t AuthenticatedAccountRegistry::GetCount() const noexcept
	{
		return accountTable_.size();
	}
}