#include "PersistenceRuntime.h"

#include <Persistence/Schema/DatabaseSchema.h>

namespace persistence
{
	PersistenceRuntime::StartResult PersistenceRuntime::Start(const PersistenceRuntimeStartConfig& startConfig)
	{
		Stop();

		enabled_ = startConfig.enabled;
		if (!enabled_)
		{
			return {};
		}

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
			return std::unexpected(openResult.error());
		}

		const odbc::OdbcConnection::HealthCheckResult healthCheckResult = connection_.ExecuteHealthCheck();
		if (!healthCheckResult.has_value())
		{
			return std::unexpected(healthCheckResult.error());
		}

		const schema::DatabaseSchema::InitializeResult schemaInitializeResult = schema::DatabaseSchema::Initialize(connection_);
		if (!schemaInitializeResult.has_value())
		{
			return std::unexpected(schemaInitializeResult.error());
		}

		return {};
	}

	void PersistenceRuntime::Stop() noexcept
	{
		connection_.Close();
		environment_.Close();
		enabled_ = false;
	}

	PersistenceRuntime::CreateAccountResult PersistenceRuntime::CreateAccount(const account::AccountCreateRequest& request)
	{
		account::AccountRepository repository(connection_);
		return repository.CreateAccount(request);
	}

	PersistenceRuntime::FindAccountResult PersistenceRuntime::FindAccountByLoginName(std::string_view loginName)
	{
		account::AccountRepository repository(connection_);
		return repository.FindAccountByLoginName(loginName);
	}

	PersistenceRuntime::ExistsAccountResult PersistenceRuntime::ExistsByLoginName(std::string_view loginName)
	{
		account::AccountRepository repository(connection_);
		return repository.ExistsByLoginName(loginName);
	}
}