#include "AccountService.h"

#include <utility>

namespace server::account
{
	AccountService::AccountService(persistence::PersistenceRuntime& persistenceRuntime) noexcept
		: persistenceRuntime_(persistenceRuntime)
	{}

	bool AccountService::IsDuplicateLoginNameError(const persistence::core::DatabaseError& databaseError) noexcept
	{
		return persistence::core::ContainsNativeError(databaseError, duplicateIndexNativeError)
			|| persistence::core::ContainsNativeError(databaseError, uniqueConstraintNativeError);
	}

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
			if (IsDuplicateLoginNameError(createResult.error()))
			{
				return std::unexpected(CreateAccountError{ CreateAccountFailure::DuplicateLoginName });
			}

			return std::unexpected(CreateAccountError{ createResult.error() });
		}

		return std::move(*createResult);
	}

	LoginAccountResult AccountService::LoginAccount(const AccountLoginRequest& request)
	{
		const ::persistence::account::AccountValidationResult validationResult = persistence::account::ValidateAccountLoginFields(
			request.loginName,
			request.passwordHash
		);
		if (!validationResult.has_value())
		{
			return std::unexpected(LoginAccountError{ validationResult.error() });
		}

		persistence::PersistenceRuntime::FindAccountResult findResult = persistenceRuntime_.FindAccountByLoginName(request.loginName);
		if (!findResult.has_value())
		{
			return std::unexpected(LoginAccountError{ findResult.error() });
		}

		if (!findResult->has_value())
		{
			return std::unexpected(LoginAccountError{ LoginAccountFailure::InvalidCredentials });
		}

		persistence::account::AccountRecord account = std::move(**findResult);

		const bool passwordHashVerified = passwordHashVerifier_.Verify(request.passwordHash, account.passwordHash);
		if (!passwordHashVerified)
		{
			return std::unexpected(LoginAccountError{ LoginAccountFailure::InvalidCredentials });
		}

		const persistence::PersistenceRuntime::FindOrCreatePlayerResult playerResult = persistenceRuntime_.FindOrCreatePlayerByAccountId(account.accountId);
		if (!playerResult.has_value())
		{
			return std::unexpected(LoginAccountError{ playerResult.error() });
		}

		if (playerResult->playerId <= 0 || playerResult->accountId != account.accountId)
		{
			return std::unexpected(LoginAccountError{
				persistence::core::DatabaseError{
					.failure = persistence::core::DatabaseFailure::StatementDataReadFailed,
					.message = "Persistent player record does not match the authenticated account.",
				}
				});
		}

		return AccountLoginRecord{
			.accountId = account.accountId,
			.persistentPlayerId = playerResult->playerId,
			.loginName = std::move(account.loginName),
			.nickname = std::move(account.nickname),
		};
	}
}