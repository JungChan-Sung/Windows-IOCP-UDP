#include "PersistenceRuntime.h"

#include <utility>

#include <Common/Identity/IdentityTypes.h>

#include <Persistence/Schema/DatabaseSchema.h>

namespace persistence
{
	core::DatabaseError PersistenceRuntime::MakeNotStartedError()
	{
		return core::DatabaseError{
			.failure = core::DatabaseFailure::ConnectionOpenFailed,
			.message = "Persistence runtime is not started.",
		};
	}

	bool PersistenceRuntime::IsDuplicateConstraintError(const core::DatabaseError& databaseError) noexcept
	{
		return core::ContainsNativeError(databaseError, duplicateIndexNativeError)
			|| core::ContainsNativeError(databaseError, uniqueConstraintNativeError);
	}

	PersistenceRuntime::StartResult PersistenceRuntime::Start(const PersistenceRuntimeStartConfig& startConfig)
	{
		std::scoped_lock lock(databaseMutex_);

		StopUnlocked();

		if (!startConfig.enabled)
		{
			return {};
		}

		enabled_ = true;

		if (startConfig.connectionString.empty())
		{
			StopUnlocked();

			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::ConnectionOpenFailed,
				.message = "Database connection string is empty.",
				});
		}

		const odbc::OdbcEnvironment::InitializeResult initializeResult = environment_.Initialize();
		if (!initializeResult.has_value())
		{
			StopUnlocked();
			return std::unexpected(initializeResult.error());
		}

		const odbc::OdbcConnection::OpenResult openResult = connection_.Open(
			environment_,
			odbc::OdbcConnectionOpenConfig{
				.connectionString = startConfig.connectionString,
				.connectionTimeoutSeconds = startConfig.connectionTimeoutSeconds,
			});
		if (!openResult.has_value())
		{
			StopUnlocked();
			return std::unexpected(openResult.error());
		}

		const odbc::OdbcConnection::HealthCheckResult healthCheckResult = connection_.ExecuteHealthCheck();
		if (!healthCheckResult.has_value())
		{
			StopUnlocked();
			return std::unexpected(healthCheckResult.error());
		}

		const schema::DatabaseSchema::InitializeResult schemaInitializeResult
			= schema::DatabaseSchema::Initialize(connection_);

		if (!schemaInitializeResult.has_value())
		{
			StopUnlocked();
			return std::unexpected(schemaInitializeResult.error());
		}

		return {};
	}

	void PersistenceRuntime::Stop() noexcept
	{
		std::scoped_lock lock(databaseMutex_);

		StopUnlocked();
	}

	PersistenceRuntime::CreateAccountResult PersistenceRuntime::CreateAccount(const account::AccountCreateRequest& request)
	{
		std::scoped_lock lock(databaseMutex_);

		if (!enabled_ || !IsStartedUnlocked())
		{
			return std::unexpected(MakeNotStartedError());
		}

		account::AccountRepository repository(connection_);
		return repository.CreateAccount(request);
	}

	PersistenceRuntime::FindAccountResult PersistenceRuntime::FindAccountByLoginName(std::string_view loginName)
	{
		std::scoped_lock lock(databaseMutex_);

		if (!enabled_ || !IsStartedUnlocked())
		{
			return std::unexpected(MakeNotStartedError());
		}

		account::AccountRepository repository(connection_);
		return repository.FindAccountByLoginName(loginName);
	}

	PersistenceRuntime::ExistsAccountResult PersistenceRuntime::ExistsByLoginName(std::string_view loginName)
	{
		std::scoped_lock lock(databaseMutex_);

		if (!enabled_ || !IsStartedUnlocked())
		{
			return std::unexpected(MakeNotStartedError());
		}

		account::AccountRepository repository(connection_);
		return repository.ExistsByLoginName(loginName);
	}

	PersistenceRuntime::CreatePlayerResult PersistenceRuntime::CreatePlayer(common::identity::AccountId accountId)
	{
		std::scoped_lock lock(databaseMutex_);

		if (!enabled_ || !IsStartedUnlocked())
		{
			return std::unexpected(MakeNotStartedError());
		}

		player::PlayerRepository repository(connection_);
		return repository.CreatePlayer(accountId);
	}

	PersistenceRuntime::FindPlayerResult PersistenceRuntime::FindPlayerByAccountId(common::identity::AccountId accountId)
	{
		std::scoped_lock lock(databaseMutex_);

		if (!enabled_ || !IsStartedUnlocked())
		{
			return std::unexpected(MakeNotStartedError());
		}

		player::PlayerRepository repository(connection_);
		return repository.FindPlayerByAccountId(accountId);
	}

	PersistenceRuntime::FindOrCreatePlayerResult PersistenceRuntime::FindOrCreatePlayerByAccountId(common::identity::AccountId accountId)
	{
		std::scoped_lock lock(databaseMutex_);

		if (!enabled_ || !IsStartedUnlocked())
		{
			return std::unexpected(MakeNotStartedError());
		}

		player::PlayerRepository repository(connection_);
		player::PlayerRepository::FindPlayerResult findResult = repository.FindPlayerByAccountId(accountId);
		if (!findResult.has_value())
		{
			return std::unexpected(findResult.error());
		}

		if (findResult->has_value())
		{
			return std::move(**findResult);
		}

		player::PlayerRepository::CreatePlayerResult createResult = repository.CreatePlayer(accountId);
		if (createResult.has_value())
		{
			return std::move(*createResult);
		}

		const core::DatabaseError createError = createResult.error();
		if (!IsDuplicateConstraintError(createError))
		{
			return std::unexpected(createError);
		}

		findResult = repository.FindPlayerByAccountId(accountId);
		if (!findResult.has_value())
		{
			return std::unexpected(findResult.error());
		}

		if (!findResult->has_value())
		{
			return std::unexpected(createError);
		}

		return std::move(**findResult);
	}

	PersistenceRuntime::SaveMatchResult PersistenceRuntime::SaveMatch(const match::MatchCreateRequest& request)
	{
		std::scoped_lock lock(databaseMutex_);

		if (!enabled_ || !IsStartedUnlocked())
		{
			return std::unexpected(MakeNotStartedError());
		}

		match::MatchHistoryRepository repository(connection_);
		return repository.SaveMatch(request);
	}

	void PersistenceRuntime::StopUnlocked() noexcept
	{
		connection_.Close();
		environment_.Close();

		enabled_ = false;
	}

	bool PersistenceRuntime::IsEnabled() const
	{
		std::scoped_lock lock(databaseMutex_);

		return enabled_;
	}

	bool PersistenceRuntime::IsStarted() const
	{
		std::scoped_lock lock(databaseMutex_);

		return IsStartedUnlocked();
	}
}