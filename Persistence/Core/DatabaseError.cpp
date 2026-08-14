#include "DatabaseError.h"

#include <sstream>

namespace persistence::core
{
	std::string_view ToString(DatabaseFailure failure) noexcept
	{
		switch (failure)
		{
		case DatabaseFailure::EnvironmentAllocationFailed:
			return "EnvironmentAllocationFailed";

		case DatabaseFailure::EnvironmentVersionSetFailed:
			return "EnvironmentVersionSetFailed";

		case DatabaseFailure::ConnectionAllocationFailed:
			return "ConnectionAllocationFailed";

		case DatabaseFailure::ConnectionOpenFailed:
			return "ConnectionOpenFailed";

		case DatabaseFailure::TransactionBeginFailed:
			return "TransactionBeginFailed";

		case DatabaseFailure::TransactionCommitFailed:
			return "TransactionCommitFailed";

		case DatabaseFailure::TransactionRollbackFailed:
			return "TransactionRollbackFailed";

		case DatabaseFailure::StatementAllocationFailed:
			return "StatementAllocationFailed";

		case DatabaseFailure::StatementPrepareFailed:
			return "StatementPrepareFailed";

		case DatabaseFailure::StatementParameterBindFailed:
			return "StatementParameterBindFailed";

		case DatabaseFailure::StatementExecutionFailed:
			return "StatementExecutionFailed";

		case DatabaseFailure::StatementFetchFailed:
			return "StatementFetchFailed";

		case DatabaseFailure::StatementDataReadFailed:
			return "StatementDataReadFailed";

		case DatabaseFailure::HealthCheckFailed:
			return "HealthCheckFailed";

		case DatabaseFailure::TextConversionFailed:
			return "TextConversionFailed";

		default:
			return "Unknown";
		}
	}

	std::string ToString(const DatabaseError& databaseError)
	{
		std::ostringstream stream;
		stream << ToString(databaseError.failure);

		if (!databaseError.message.empty())
		{
			stream << ": " << databaseError.message;
		}

		return stream.str();
	}

	bool ContainsNativeError(const DatabaseError& databaseError, std::int32_t nativeError) noexcept
	{
		for (const DatabaseDiagnosticRecord& diagnosticRecord : databaseError.diagnosticRecordList)
		{
			if (diagnosticRecord.nativeError == nativeError)
			{
				return true;
			}
		}

		return false;
	}
}