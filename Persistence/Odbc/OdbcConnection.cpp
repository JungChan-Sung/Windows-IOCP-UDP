#include "OdbcConnection.h"

#include <sqlext.h>

#include <cstdint>
#include <string>

#include <Persistence/Odbc/OdbcDiagnostic.h>

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

		SQLHSTMT statementHandle = SQL_NULL_HSTMT;

		const SQLRETURN allocationResult = ::SQLAllocHandle(
			SQL_HANDLE_STMT,
			connectionHandle_,
			&statementHandle
		);

		if (!SQL_SUCCEEDED(allocationResult))
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::StatementAllocationFailed,
				.handleType = SQL_HANDLE_DBC,
				.handle = connectionHandle_,
				.message = "Failed to allocate ODBC statement handle.",
				}));
		}

		char query[] = "SELECT 1";

		const SQLRETURN executeResult = ::SQLExecDirectA(
			statementHandle,
			reinterpret_cast<SQLCHAR*>(query),
			SQL_NTS
		);

		if (!SQL_SUCCEEDED(executeResult))
		{
			const core::DatabaseError error = MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::HealthCheckFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle,
				.message = "Failed to execute database health check query.",
				});

			::SQLFreeHandle(SQL_HANDLE_STMT, statementHandle);
			return std::unexpected(error);
		}

		const SQLRETURN fetchResult = ::SQLFetch(statementHandle);
		if (!SQL_SUCCEEDED(fetchResult))
		{
			const core::DatabaseError error = MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::HealthCheckFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle,
				.message = "Database health check query returned no row.",
				});

			::SQLFreeHandle(SQL_HANDLE_STMT, statementHandle);
			return std::unexpected(error);
		}

		SQLLEN indicator = 0;
		SQLINTEGER value = 0;

		const SQLRETURN getDataResult = ::SQLGetData(
			statementHandle,
			static_cast<SQLUSMALLINT>(1),
			SQL_C_SLONG,
			static_cast<SQLPOINTER>(&value),
			static_cast<SQLLEN>(sizeof(value)),
			&indicator
		);

		if (!SQL_SUCCEEDED(getDataResult) || value != 1)
		{
			const core::DatabaseError error = MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::HealthCheckFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle,
				.message = "Database health check query returned an invalid value.",
				});

			::SQLFreeHandle(SQL_HANDLE_STMT, statementHandle);
			return std::unexpected(error);
		}

		::SQLFreeHandle(SQL_HANDLE_STMT, statementHandle);
		return {};
	}
}