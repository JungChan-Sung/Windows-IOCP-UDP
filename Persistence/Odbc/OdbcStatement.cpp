#include "OdbcStatement.h"

#include <string>

#include <Persistence/Odbc/OdbcDiagnostic.h>

namespace persistence::odbc
{
	OdbcStatement::~OdbcStatement() noexcept
	{
		Close();
	}

	OdbcStatement::ExecuteResult OdbcStatement::ExecuteDirect(const OdbcConnection& connection, std::string_view query)
	{
		Close();

		SQLHSTMT statementHandle = SQL_NULL_HSTMT;

		const SQLRETURN allocationResult = ::SQLAllocHandle(
			SQL_HANDLE_STMT,
			connection.GetHandle(),
			&statementHandle
		);
		if (!SQL_SUCCEEDED(allocationResult))
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::StatementAllocationFailed,
				.handleType = SQL_HANDLE_DBC,
				.handle = connection.GetHandle(),
				.message = "Failed to allocate ODBC statement handle.",
				}));
		}

		std::string queryText(query);

		const SQLRETURN executeResult = ::SQLExecDirectA(
			statementHandle,
			reinterpret_cast<SQLCHAR*>(queryText.data()),
			SQL_NTS
		);
		if (!SQL_SUCCEEDED(executeResult))
		{
			const core::DatabaseError error = MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::StatementExecutionFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle,
				.message = "Failed to execute ODBC statement.",
				});

			::SQLFreeHandle(SQL_HANDLE_STMT, statementHandle);
			return std::unexpected(error);
		}

		statementHandle_ = statementHandle;
		return {};
	}

	OdbcStatement::FetchResult OdbcStatement::Fetch()
	{
		const SQLRETURN fetchResult = ::SQLFetch(statementHandle_);
		if (fetchResult == SQL_NO_DATA)
		{
			return false;
		}

		if (!SQL_SUCCEEDED(fetchResult))
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::StatementFetchFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle_,
				.message = "Failed to fetch ODBC statement row.",
				}));
		}

		return true;
	}

	OdbcStatement::ReadInt32Result OdbcStatement::ReadInt32(SQLUSMALLINT columnNumber)
	{
		SQLLEN indicator = 0;
		SQLINTEGER value = 0;

		const SQLRETURN getDataResult = ::SQLGetData(
			statementHandle_,
			columnNumber,
			SQL_C_SLONG,
			static_cast<SQLPOINTER>(&value),
			static_cast<SQLLEN>(sizeof(value)),
			&indicator
		);
		if (!SQL_SUCCEEDED(getDataResult) || indicator == SQL_NULL_DATA)
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::StatementDataReadFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle_,
				.message = "Failed to read ODBC int32 column.",
				}));
		}

		return static_cast<int>(value);
	}

	void OdbcStatement::Close() noexcept
	{
		if (statementHandle_ == SQL_NULL_HSTMT)
		{
			return;
		}

		::SQLFreeHandle(SQL_HANDLE_STMT, statementHandle_);
		statementHandle_ = SQL_NULL_HSTMT;
	}
}