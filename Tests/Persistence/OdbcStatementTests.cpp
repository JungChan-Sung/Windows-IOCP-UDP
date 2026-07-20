#include "OdbcStatementTests.h"

#include <string>
#include <string_view>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>
#include <Persistence/Odbc/OdbcStatement.h>

#include <Tests/DebugTestResult.h>

namespace
{
	template <typename TResult>
	void ExpectFailure(
		tests::DebugTestResult& result,
		const TResult& operationResult,
		::persistence::core::DatabaseFailure expectedFailure,
		std::string_view testName
	)
	{
		const bool hasExpectedFailure
			= !operationResult.has_value()
			&& operationResult.error().failure
			== expectedFailure;

		tests::Expect(
			result,
			hasExpectedFailure,
			std::string{ testName }
		);
	}

	void RunClosedConnectionTests(
		tests::DebugTestResult& result
	)
	{
		::persistence::odbc::OdbcConnection connection;
		::persistence::odbc::OdbcStatement statement;

		ExpectFailure(
			result,
			statement.ExecuteDirect(
				connection,
				"SELECT 1"
			),
			::persistence::core::DatabaseFailure
			::ConnectionOpenFailed,
			"OdbcStatement: reject ExecuteDirect with "
			"closed connection"
		);

		ExpectFailure(
			result,
			statement.Prepare(
				connection,
				"SELECT 1"
			),
			::persistence::core::DatabaseFailure
			::ConnectionOpenFailed,
			"OdbcStatement: reject Prepare with "
			"closed connection"
		);
	}

	void RunClosedStatementTests(
		tests::DebugTestResult& result
	)
	{
		::persistence::odbc::OdbcStatement statement;

		ExpectFailure(
			result,
			statement.BindInputString(1, "value"),
			::persistence::core::DatabaseFailure
			::StatementParameterBindFailed,
			"OdbcStatement: reject string bind before prepare"
		);

		ExpectFailure(
			result,
			statement.BindInputInt64(1, 1),
			::persistence::core::DatabaseFailure
			::StatementParameterBindFailed,
			"OdbcStatement: reject int64 bind before prepare"
		);

		ExpectFailure(
			result,
			statement.Execute(),
			::persistence::core::DatabaseFailure
			::StatementExecutionFailed,
			"OdbcStatement: reject execute before prepare"
		);

		ExpectFailure(
			result,
			statement.Fetch(),
			::persistence::core::DatabaseFailure
			::StatementFetchFailed,
			"OdbcStatement: reject fetch before prepare"
		);

		ExpectFailure(
			result,
			statement.ReadInt32(1),
			::persistence::core::DatabaseFailure
			::StatementDataReadFailed,
			"OdbcStatement: reject int32 read before prepare"
		);

		ExpectFailure(
			result,
			statement.ReadInt64(1),
			::persistence::core::DatabaseFailure
			::StatementDataReadFailed,
			"OdbcStatement: reject int64 read before prepare"
		);

		ExpectFailure(
			result,
			statement.ReadString(1),
			::persistence::core::DatabaseFailure
			::StatementDataReadFailed,
			"OdbcStatement: reject string read before prepare"
		);
	}
}

namespace tests::persistence
{
	DebugTestResult RunOdbcStatementTests()
	{
		DebugTestResult result{};

		RunClosedConnectionTests(result);
		RunClosedStatementTests(result);

		return result;
	}
}