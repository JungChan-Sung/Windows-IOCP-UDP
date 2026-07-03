#include "OdbcEnvironment.h"

#include <sqlext.h>

#include <cstdint>

#include <Persistence/Odbc/OdbcDiagnostic.h>

namespace persistence::odbc
{
	OdbcEnvironment::~OdbcEnvironment() noexcept
	{
		Close();
	}

	OdbcEnvironment::InitializeResult OdbcEnvironment::Initialize()
	{
		if (IsInitialized())
		{
			return {};
		}

		SQLHENV environmentHandle = SQL_NULL_HENV;

		const SQLRETURN allocationResult = ::SQLAllocHandle(
			SQL_HANDLE_ENV,
			SQL_NULL_HANDLE,
			&environmentHandle
		);
		if (!SQL_SUCCEEDED(allocationResult))
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::EnvironmentAllocationFailed,
				.message = "Failed to allocate ODBC environment handle.",
				});
		}

		const SQLRETURN versionResult = ::SQLSetEnvAttr(
			environmentHandle,
			SQL_ATTR_ODBC_VERSION,
			reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3),
			0
		);
		if (!SQL_SUCCEEDED(versionResult))
		{
			const core::DatabaseError error = MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::EnvironmentVersionSetFailed,
				.handleType = SQL_HANDLE_ENV,
				.handle = environmentHandle,
				.message = "Failed to set ODBC environment version.",
				});

			::SQLFreeHandle(SQL_HANDLE_ENV, environmentHandle);
			return std::unexpected(error);
		}

		environmentHandle_ = environmentHandle;
		return {};
	}

	void OdbcEnvironment::Close() noexcept
	{
		if (environmentHandle_ == SQL_NULL_HENV)
		{
			return;
		}

		::SQLFreeHandle(SQL_HANDLE_ENV, environmentHandle_);
		environmentHandle_ = SQL_NULL_HENV;
	}
}