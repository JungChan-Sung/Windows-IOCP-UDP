#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace  persistence::core
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

	struct DatabaseDiagnosticRecord
	{
	public:
		std::string sqlState;
		std::int32_t nativeError = 0;
		std::string message;
	};

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