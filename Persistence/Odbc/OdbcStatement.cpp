#include "OdbcStatement.h"

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include <Common/String/UtfConversion.h>

#include <Persistence/Odbc/OdbcDiagnostic.h>

namespace
{
	persistence::core::DatabaseError MakeTextConversionError(std::string_view message, std::uint32_t nativeError)
	{
		std::string errorMessage(message);
		errorMessage += " NativeError=";
		errorMessage += std::to_string(nativeError);

		return persistence::core::DatabaseError{
			.failure = persistence::core::DatabaseFailure::TextConversionFailed,
			.message = std::move(errorMessage),
		};
	}
}

namespace persistence::odbc
{
	OdbcStatement::~OdbcStatement() noexcept
	{
		Close();
	}

	OdbcStatement::ExecuteResult OdbcStatement::ExecuteDirect(const OdbcConnection& connection, std::string_view query)
	{
		Close();

		if (!connection.IsOpen())
		{
			return std::unexpected(core::DatabaseError{
					.failure = core::DatabaseFailure::ConnectionOpenFailed,
					.message = "ODBC connection is not open.",
				});
		}

		common::string::Utf16ConversionResult queryTextResult = common::string::ConvertUtf8ToUtf16(query);
		if (!queryTextResult.has_value())
		{
			return std::unexpected(MakeTextConversionError(
				"Failed to convert direct ODBC query from UTF-8 to UTF-16.",
				queryTextResult.error().nativeError
			));
		}

		std::wstring& queryText = *queryTextResult;

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

		const SQLRETURN executeResult = ::SQLExecDirectW(
			statementHandle,
			reinterpret_cast<SQLWCHAR*>(queryText.data()),
			SQL_NTS
		);
		if (executeResult == SQL_NO_DATA)
		{
			statementHandle_ = statementHandle;
			return {};
		}

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

		if (!connection.IsOpen())
		{
			return std::unexpected(core::DatabaseError{
					.failure = core::DatabaseFailure::ConnectionOpenFailed,
					.message = "ODBC connection is not open.",
				});
		}

		common::string::Utf16ConversionResult queryTextResult = common::string::ConvertUtf8ToUtf16(query);
		if (!queryTextResult.has_value())
		{
			return std::unexpected(MakeTextConversionError(
				"Failed to convert prepared ODBC query from UTF-8 to UTF-16.",
				queryTextResult.error().nativeError
			));
		}

		std::wstring& queryText = *queryTextResult;

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

		const SQLRETURN prepareResult = ::SQLPrepareW(
			statementHandle,
			reinterpret_cast<SQLWCHAR*>(queryText.data()),
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

		common::string::Utf16ConversionResult valueResult = common::string::ConvertUtf8ToUtf16(value);
		if (!valueResult.has_value())
		{
			return std::unexpected(MakeTextConversionError(
				"Failed to convert ODBC string parameter from UTF-8 to UTF-16.",
				valueResult.error().nativeError
			));
		}

		boundStringParameters_.push_back(BoundStringParameter{
			.value = std::move(*valueResult),
			});

		BoundStringParameter& parameter = boundStringParameters_.back();
		const SQLULEN columnSize = static_cast<SQLULEN>(parameter.value.empty() ? 1 : parameter.value.size());
		const SQLLEN bufferLength = static_cast<SQLLEN>((parameter.value.size() + 1) * sizeof(wchar_t));

		const SQLRETURN bindResult = ::SQLBindParameter(
			statementHandle_,
			parameterNumber,
			SQL_PARAM_INPUT,
			SQL_C_WCHAR,
			SQL_WVARCHAR,
			columnSize,
			0,
			static_cast<SQLPOINTER>(parameter.value.data()),
			bufferLength,
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

		if (executeResult == SQL_NO_DATA)
		{
			return {};
		}

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
		if (!IsOpen())
		{
			return std::unexpected(core::DatabaseError{
					.failure = core::DatabaseFailure::StatementFetchFailed,
					.message = "ODBC statement is not open.",
				});
		}

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
		if (!IsOpen())
		{
			return std::unexpected(core::DatabaseError{
					.failure = core::DatabaseFailure::StatementDataReadFailed,
					.message = "ODBC statement is not open.",
				});
		}

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
		if (!IsOpen())
		{
			return std::unexpected(core::DatabaseError{
					.failure = core::DatabaseFailure::StatementDataReadFailed,
					.message = "ODBC statement is not open.",
				});
		}

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
		if (!IsOpen())
		{
			return std::unexpected(core::DatabaseError{
					.failure = core::DatabaseFailure::StatementDataReadFailed,
					.message = "ODBC statement is not open.",
				});
		}

		constexpr std::size_t bufferCharacterCount = 256;

		std::wstring value;

		while (true)
		{
			std::array<wchar_t, bufferCharacterCount> buffer{};
			SQLLEN indicator = 0;

			const SQLRETURN getDataResult = ::SQLGetData(
				statementHandle_,
				columnNumber,
				SQL_C_WCHAR,
				static_cast<SQLPOINTER>(buffer.data()),
				static_cast<SQLLEN>(sizeof(buffer)),
				&indicator
			);
			if (getDataResult == SQL_NO_DATA)
			{
				break;
			}

			if (!SQL_SUCCEEDED(getDataResult) || indicator == SQL_NULL_DATA)
			{
				return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
					.failure = core::DatabaseFailure::StatementDataReadFailed,
					.handleType = SQL_HANDLE_STMT,
					.handle = statementHandle_,
					.message = "Failed to read ODBC string column.",
					}));
			}

			const std::size_t chunkLength = std::char_traits<wchar_t>::length(buffer.data());
			value.append(buffer.data(), chunkLength);

			if (getDataResult == SQL_SUCCESS)
			{
				break;
			}
		}

		const common::string::Utf8ConversionResult valueResult = common::string::ConvertUtf16ToUtf8(value);
		if (!valueResult.has_value())
		{
			return std::unexpected(MakeTextConversionError(
				"Failed to convert ODBC string column from UTF-16 to UTF-8.",
				valueResult.error().nativeError
			));
		}

		return *valueResult;
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