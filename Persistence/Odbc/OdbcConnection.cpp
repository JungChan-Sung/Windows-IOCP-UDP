#include "OdbcConnection.h"

#include <sqlext.h>

#include <cstdint>
#include <string>

#include <Persistence/Odbc/OdbcDiagnostic.h>
#include <Persistence/Odbc/OdbcStatement.h>

namespace persistence::odbc
{
	OdbcConnection::~OdbcConnection() noexcept
	{
		Close();
	}

	OdbcConnection::OpenResult OdbcConnection::Open(const OdbcEnvironment& environment, const OdbcConnectionOpenConfig& openConfig)
	{
		Close();

		SQLHDBC connectionHandle = SQL_NULL_HDBC;

		const SQLRETURN allocationResult = ::SQLAllocHandle(
			SQL_HANDLE_DBC,
			environment.GetHandle(),
			&connectionHandle
		);
		if (!SQL_SUCCEEDED(allocationResult))
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::ConnectionAllocationFailed,
				.message = "Failed to allocate ODBC connection handle.",
				});
		}

		if (openConfig.connectionTimeoutSeconds > 0)
		{
			const SQLRETURN timeoutResult = ::SQLSetConnectAttrA(
				connectionHandle,
				SQL_LOGIN_TIMEOUT,
				reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(openConfig.connectionTimeoutSeconds)),
				0
			);

			if (!SQL_SUCCEEDED(timeoutResult))
			{
				const core::DatabaseError error = MakeOdbcError(OdbcDiagnosticContext{
					.failure = core::DatabaseFailure::ConnectionOpenFailed,
					.handleType = SQL_HANDLE_DBC,
					.handle = connectionHandle,
					.message = "Failed to set ODBC login timeout.",
					});

				::SQLFreeHandle(SQL_HANDLE_DBC, connectionHandle);
				return std::unexpected(error);
			}
		}

		std::string connectionString(openConfig.connectionString);

		const SQLRETURN connectResult = ::SQLDriverConnectA(
			connectionHandle,
			nullptr,
			reinterpret_cast<SQLCHAR*>(connectionString.data()),
			SQL_NTS,
			nullptr,
			0,
			nullptr,
			SQL_DRIVER_NOPROMPT
		);
		if (!SQL_SUCCEEDED(connectResult))
		{
			const core::DatabaseError error = MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::ConnectionOpenFailed,
				.handleType = SQL_HANDLE_DBC,
				.handle = connectionHandle,
				.message = "Failed to open ODBC connection.",
				});

			::SQLFreeHandle(SQL_HANDLE_DBC, connectionHandle);
			return std::unexpected(error);
		}

		connectionHandle_ = connectionHandle;
		return {};
	}

	void OdbcConnection::Close() noexcept
	{
		if (connectionHandle_ == SQL_NULL_HDBC)
		{
			return;
		}

		::SQLDisconnect(connectionHandle_);
		::SQLFreeHandle(SQL_HANDLE_DBC, connectionHandle_);
		connectionHandle_ = SQL_NULL_HDBC;
	}

	OdbcConnection::HealthCheckResult OdbcConnection::ExecuteHealthCheck() const
	{
		if (!IsOpen())
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::HealthCheckFailed,
				.message = "Database connection is not open.",
				});
		}

		OdbcStatement statement;

		const OdbcStatement::ExecuteResult prepareResult = statement.Prepare(*this, "SELECT ?");
		if (!prepareResult.has_value())
		{
			return std::unexpected(prepareResult.error());
		}

		const OdbcStatement::BindResult bindResult = statement.BindInputInt64(static_cast<SQLUSMALLINT>(1), 1);
		if (!bindResult.has_value())
		{
			return std::unexpected(bindResult.error());
		}

		const OdbcStatement::ExecuteResult executeResult = statement.Execute();
		if (!executeResult.has_value())
		{
			return std::unexpected(executeResult.error());
		}

		return {};
	}
}