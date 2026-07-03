#pragma once

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcDiagnosticContext.h>

namespace persistence::odbc
{
	[[nodiscard]] core::DatabaseError MakeOdbcError(const OdbcDiagnosticContext& context);
}