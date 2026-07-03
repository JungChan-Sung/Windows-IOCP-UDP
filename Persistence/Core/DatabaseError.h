#pragma once

#include <string>
#include <string_view>

namespace persistence::core
{
	enum class DatabaseFailure
	{
		EnvironmentAllocationFailed,
		EnvironmentVersionSetFailed,
		ConnectionAllocationFailed,
		ConnectionOpenFailed,
		StatementAllocationFailed,
		HealthCheckFailed,
	};

	struct DatabaseError
	{
	public:
		DatabaseFailure failure = DatabaseFailure::ConnectionOpenFailed;
		std::string message;
	};

	[[nodiscard]] std::string_view ToString(DatabaseFailure failure) noexcept;
	[[nodiscard]] std::string ToString(const DatabaseError& databaseError);
}