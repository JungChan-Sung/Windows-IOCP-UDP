#include "OdbcStatement.h"

#include <array>
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

	OdbcStatement::ExecuteResult OdbcStatement::Prepare(const OdbcConnection& connection, std::string_view query)
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

		const SQLRETURN prepareResult = ::SQLPrepareA(
			statementHandle,
			reinterpret_cast<SQLCHAR*>(queryText.data()),
			SQL_NTS
		);
		if (!SQL_SUCCEEDED(prepareResult))
		{
			const core::DatabaseError error = MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::StatementPrepareFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle,
				.message = "Failed to prepare ODBC statement.",
				});

			::SQLFreeHandle(SQL_HANDLE_STMT, statementHandle);
			return std::unexpected(error);
		}

		statementHandle_ = statementHandle;
		return {};
	}

	OdbcStatement::BindResult OdbcStatement::BindInputString(SQLUSMALLINT parameterNumber, std::string_view value)
	{
		if (!IsOpen())
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::StatementParameterBindFailed,
				.message = "ODBC statement is not prepared.",
				});
		}

		boundStringParameters_.push_back(BoundStringParameter{
			.value = std::string(value),
			});

		BoundStringParameter& parameter = boundStringParameters_.back();

		const SQLULEN columnSize = static_cast<SQLULEN>(parameter.value.empty() ? 1 : parameter.value.size());

		const SQLRETURN bindResult = ::SQLBindParameter(
			statementHandle_,
			parameterNumber,
			SQL_PARAM_INPUT,
			SQL_C_CHAR,
			SQL_VARCHAR,
			columnSize,
			0,
			static_cast<SQLPOINTER>(parameter.value.data()),
			static_cast<SQLLEN>(parameter.value.size() + 1),
			&parameter.indicator
		);
		if (!SQL_SUCCEEDED(bindResult))
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::StatementParameterBindFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle_,
				.message = "Failed to bind ODBC string parameter.",
				}));
		}

		return {};
	}

	OdbcStatement::BindResult OdbcStatement::BindInputInt64(SQLUSMALLINT parameterNumber, std::int64_t value)
	{
		if (!IsOpen())
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::StatementParameterBindFailed,
				.message = "ODBC statement is not prepared.",
				});
		}

		boundInt64Parameters_.push_back(BoundInt64Parameter{
			.value = static_cast<SQLBIGINT>(value),
			});

		BoundInt64Parameter& parameter = boundInt64Parameters_.back();

		const SQLRETURN bindResult = ::SQLBindParameter(
			statementHandle_,
			parameterNumber,
			SQL_PARAM_INPUT,
			SQL_C_SBIGINT,
			SQL_BIGINT,
			0,
			0,
			static_cast<SQLPOINTER>(&parameter.value),
			0,
			&parameter.indicator
		);
		if (!SQL_SUCCEEDED(bindResult))
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::StatementParameterBindFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle_,
				.message = "Failed to bind ODBC int64 parameter.",
				}));
		}

		return {};
	}

	OdbcStatement::ExecuteResult OdbcStatement::Execute()
	{
		if (!IsOpen())
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::StatementExecutionFailed,
				.message = "ODBC statement is not prepared.",
				});
		}

		const SQLRETURN executeResult = ::SQLExecute(statementHandle_);
		if (!SQL_SUCCEEDED(executeResult))
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::StatementExecutionFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle_,
				.message = "Failed to execute prepared ODBC statement.",
				}));
		}

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

	OdbcStatement::ReadInt64Result OdbcStatement::ReadInt64(SQLUSMALLINT columnNumber)
	{
		SQLLEN indicator = 0;
		SQLBIGINT value = 0;

		const SQLRETURN getDataResult = ::SQLGetData(
			statementHandle_,
			columnNumber,
			SQL_C_SBIGINT,
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
				.message = "Failed to read ODBC int64 column.",
				}));
		}

		return static_cast<std::int64_t>(value);
	}

	OdbcStatement::ReadStringResult OdbcStatement::ReadString(SQLUSMALLINT columnNumber)
	{
		std::array<char, 1024> buffer{};
		SQLLEN indicator = 0;

		const SQLRETURN getDataResult = ::SQLGetData(
			statementHandle_,
			columnNumber,
			SQL_C_CHAR,
			static_cast<SQLPOINTER>(buffer.data()),
			static_cast<SQLLEN>(buffer.size()),
			&indicator
		);
		if (!SQL_SUCCEEDED(getDataResult) || indicator == SQL_NULL_DATA)
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::StatementDataReadFailed,
				.handleType = SQL_HANDLE_STMT,
				.handle = statementHandle_,
				.message = "Failed to read ODBC string column.",
				}));
		}

		return std::string(buffer.data());
	}

	void OdbcStatement::Close() noexcept
	{
		if (statementHandle_ != SQL_NULL_HSTMT)
		{
			::SQLFreeHandle(SQL_HANDLE_STMT, statementHandle_);
			statementHandle_ = SQL_NULL_HSTMT;
		}

		boundStringParameters_.clear();
		boundInt64Parameters_.clear();
	}
}