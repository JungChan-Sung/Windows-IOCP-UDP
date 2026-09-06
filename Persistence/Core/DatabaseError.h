#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace persistence::core
{
	enum class DatabaseFailure
	{
		EnvironmentAllocationFailed,
		EnvironmentVersionSetFailed,
		ConnectionAllocationFailed,
		ConnectionOpenFailed,

		TransactionBeginFailed,
		TransactionCommitFailed,
		TransactionRollbackFailed,

		StatementAllocationFailed,
		StatementPrepareFailed,
		StatementParameterBindFailed,
		StatementExecutionFailed,
		StatementFetchFailed,
		StatementDataReadFailed,
		HealthCheckFailed,

		TextConversionFailed,
	};

	// ODBC가 제공하는 SQLSTATE, Native Error, 진단 메시지를 보관하는 구조체
	struct DatabaseDiagnosticRecord
	{
	public:
		std::string sqlState;
		std::int32_t nativeError = 0;
		std::string message;
	};

	// Persistence 계층의 실패 종류와 DB Driver의 상세 진단 정보를 함께 전달하는 구조체
	struct DatabaseError
	{
	public:
		DatabaseFailure failure = DatabaseFailure::ConnectionOpenFailed;
		std::string message;
		std::vector<DatabaseDiagnosticRecord> diagnosticRecordList;
	};

	[[nodiscard]] std::string_view ToString(DatabaseFailure failure) noexcept;
	[[nodiscard]] std::string ToString(const DatabaseError& databaseError);

	[[nodiscard]] bool ContainsNativeError(const DatabaseError& databaseError, std::int32_t nativeError) noexcept;
}