#include "AccountService.h"

#include <utility>

namespace server::account
{
	AccountService::AccountService(persistence::PersistenceRuntime& persistenceRuntime) noexcept
		: persistenceRuntime_(persistenceRuntime)
	{}

	CreateAccountResult AccountService::CreateAccount(const persistence::account::AccountCreateRequest& request)
	{
		const ::persistence::account::AccountValidationResult validationResult = persistence::account::ValidateAccountCreateFields(
			request.loginName,
			request.passwordHash,
			request.nickname
		);
		if (!validationResult.has_value())
		{
			return std::unexpected(CreateAccountError{ validationResult.error() });
		}

		persistence::PersistenceRuntime::CreateAccountResult createResult = persistenceRuntime_.CreateAccount(request);
		if (!createResult.has_value())
		{
			return std::unexpected(CreateAccountError{ createResult.error() });
		}

		return std::move(*createResult);
	}
}