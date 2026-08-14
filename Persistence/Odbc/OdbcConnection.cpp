#include "OdbcConnection.h"

#include <sqlext.h>

#include <cstdint>
#include <string>

#include <Common/String/UtfConversion.h>

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

		common::string::Utf16ConversionResult connectionStringResult = common::string::ConvertUtf8ToUtf16(openConfig.connectionString);
		if (!connectionStringResult.has_value())
		{
			return std::unexpected(core::DatabaseError{
					.failure = core::DatabaseFailure::TextConversionFailed,
					.message = "Failed to convert ODBC connection string from UTF-8 to UTF-16. NativeError="
						+ std::to_string(connectionStringResult.error().nativeError),
				});
		}

		std::wstring& connectionString = *connectionStringResult;

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
			const SQLRETURN timeoutResult = ::SQLSetConnectAttrW(
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

		const SQLRETURN connectResult = ::SQLDriverConnectW(
			connectionHandle,
			nullptr,
			reinterpret_cast<SQLWCHAR*>(connectionString.data()),
			SQL_NTS,
			nullptr,
			0,
			nullptr,
			SQL_DRIVER_NOPROMPT
		);
		if (!SQL_SUCCEEDED(connectResult))
		{
			const core::DatabaseError error = MakeOdbcError(
				OdbcDiagnosticContext{
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

		if (transactionActive_)
		{
			static_cast<void>(::SQLEndTran(SQL_HANDLE_DBC, connectionHandle_, SQL_ROLLBACK));
			transactionActive_ = false;
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

	OdbcConnection::TransactionResult OdbcConnection::BeginTransaction()
	{
		if (!IsOpen())
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::TransactionBeginFailed,
				.message = "Database connection is not open.",
				});
		}

		if (transactionActive_)
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::TransactionBeginFailed,
				.message = "Database transaction is already active.",
				});
		}

		const SQLRETURN result = ::SQLSetConnectAttrW(
			connectionHandle_,
			SQL_ATTR_AUTOCOMMIT,
			reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(SQL_AUTOCOMMIT_OFF)),
			0
		);
		if (!SQL_SUCCEEDED(result))
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::TransactionBeginFailed,
				.handleType = SQL_HANDLE_DBC,
				.handle = connectionHandle_,
				.message = "Failed to begin ODBC transaction.",
				}));
		}

		transactionActive_ = true;
		return {};
	}

	OdbcConnection::TransactionResult OdbcConnection::CommitTransaction()
	{
		if (!IsOpen() || !transactionActive_)
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::TransactionCommitFailed,
				.message = "Database transaction is not active.",
				});
		}

		const SQLRETURN endResult = ::SQLEndTran(SQL_HANDLE_DBC, connectionHandle_, SQL_COMMIT);
		if (!SQL_SUCCEEDED(endResult))
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::TransactionCommitFailed,
				.handleType = SQL_HANDLE_DBC,
				.handle = connectionHandle_,
				.message = "Failed to commit ODBC transaction.",
				}));
		}

		transactionActive_ = false;

		const SQLRETURN autoCommitResult = ::SQLSetConnectAttrW(
			connectionHandle_,
			SQL_ATTR_AUTOCOMMIT,
			reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(SQL_AUTOCOMMIT_ON)),
			0
		);
		if (!SQL_SUCCEEDED(autoCommitResult))
		{
			core::DatabaseError error = MakeOdbcError(OdbcDiagnosticContext{
					.failure = core::DatabaseFailure::TransactionCommitFailed,
					.handleType = SQL_HANDLE_DBC,
					.handle = connectionHandle_,
					.message = "Transaction committed, but failed to restore ODBC auto-commit mode.",
				});

			Close();
			return std::unexpected(std::move(error));
		}

		return {};
	}

	OdbcConnection::TransactionResult OdbcConnection::RollbackTransaction()
	{
		if (!IsOpen() || !transactionActive_)
		{
			return std::unexpected(core::DatabaseError{
				.failure = core::DatabaseFailure::TransactionRollbackFailed,
				.message = "Database transaction is not active.",
				});
		}

		const SQLRETURN endResult = ::SQLEndTran(SQL_HANDLE_DBC, connectionHandle_, SQL_ROLLBACK);
		if (!SQL_SUCCEEDED(endResult))
		{
			return std::unexpected(MakeOdbcError(OdbcDiagnosticContext{
				.failure = core::DatabaseFailure::TransactionRollbackFailed,
				.handleType = SQL_HANDLE_DBC,
				.handle = connectionHandle_,
				.message = "Failed to roll back ODBC transaction.",
				}));
		}

		transactionActive_ = false;

		const SQLRETURN autoCommitResult = ::SQLSetConnectAttrW(
			connectionHandle_,
			SQL_ATTR_AUTOCOMMIT,
			reinterpret_cast<SQLPOINTER>(static_cast<std::intptr_t>(SQL_AUTOCOMMIT_ON)),
			0
		);
		if (!SQL_SUCCEEDED(autoCommitResult))
		{
			core::DatabaseError error = MakeOdbcError(OdbcDiagnosticContext{
					.failure = core::DatabaseFailure::TransactionCommitFailed,
					.handleType = SQL_HANDLE_DBC,
					.handle = connectionHandle_,
					.message = "Transaction committed, but failed to restore ODBC auto-commit mode.",
				});

			Close();
			return std::unexpected(std::move(error));
		}

		return {};
	}
}