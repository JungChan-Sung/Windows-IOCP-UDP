#include "PersistenceRuntime.h"

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