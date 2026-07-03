#pragma once

#include <Windows.h>
#include <sql.h>
#include <sqlext.h>

#include <string_view>

#include <Persistence/Core/DatabaseError.h>

namespace persistence::odbc
{
	struct OdbcDiagnosticContext
	{
	public:
		core::DatabaseFailure failure = core::DatabaseFailure::ConnectionOpenFailed;
		SQLSMALLINT handleType = SQL_HANDLE_DBC;
		SQLHANDLE handle = SQL_NULL_HANDLE;
		std::string_view message;
	};
}