#include "OdbcDiagnostic.h"

#include <sqlext.h>

#include <array>
#include <sstream>
#include <string>

namespace persistence::odbc
{
	core::DatabaseError MakeOdbcError(const OdbcDiagnosticContext& context)
	{
		std::ostringstream stream;
		stream << context.message;

		SQLSMALLINT recordNumber = 1;
		bool hasDiagnosticRecord = false;

		while (true)
		{
			std::array<SQLCHAR, 6> sqlState{};
			SQLINTEGER nativeError = 0;
			std::array<SQLCHAR, 1024> messageText{};
			SQLSMALLINT textLength = 0;

			const SQLRETURN result = ::SQLGetDiagRecA(
				context.handleType,
				context.handle,
				recordNumber,
				sqlState.data(),
				&nativeError,
				messageText.data(),
				static_cast<SQLSMALLINT>(messageText.size()),
				&textLength
			);

			if (result == SQL_NO_DATA)
			{
				break;
			}

			if (!SQL_SUCCEEDED(result))
			{
				break;
			}

			stream << (hasDiagnosticRecord ? " | " : " ")
				<< "[SQLSTATE=" << reinterpret_cast<const char*>(sqlState.data())
				<< ", NativeError=" << nativeError
				<< ", Message=" << reinterpret_cast<const char*>(messageText.data())
				<< "]";

			hasDiagnosticRecord = true;
			++recordNumber;
		}

		return core::DatabaseError{
			.failure = context.failure,
			.message = stream.str(),
		};
	}
}