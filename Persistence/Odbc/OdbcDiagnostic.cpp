#include "OdbcDiagnostic.h"

#include <sqlext.h>

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <string_view>

#include <Common/String/UtfConversion.h>

namespace
{
	std::string ConvertDiagnosticText(std::wstring_view text)
	{
		const common::string::Utf8ConversionResult conversionResult = common::string::ConvertUtf16ToUtf8(text);
		if (conversionResult.has_value())
		{
			return *conversionResult;
		}

		std::string failureText = "<UTF-16 to UTF-8 conversion failed. NativeError=";
		failureText += std::to_string(conversionResult.error().nativeError);
		failureText += '>';
		return failureText;
	}
}

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
			std::array<wchar_t, 6> sqlState{};
			SQLINTEGER nativeError = 0;
			std::array<wchar_t, 1024> messageText{};
			SQLSMALLINT textLength = 0;

			const SQLRETURN result = ::SQLGetDiagRecW(
				context.handleType,
				context.handle,
				recordNumber,
				reinterpret_cast<SQLWCHAR*>(sqlState.data()),
				&nativeError,
				reinterpret_cast<SQLWCHAR*>(messageText.data()),
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

			const std::size_t messageLength = (textLength > 0) ? std::min(static_cast<std::size_t>(textLength), messageText.size() - 1) : 0;
			const std::string sqlStateText = ConvertDiagnosticText(std::wstring_view{ sqlState.data() });
			const std::string message = ConvertDiagnosticText(std::wstring_view{ messageText.data(), messageLength });
			stream << (hasDiagnosticRecord ? " | " : " ")
				<< "[SQLSTATE=" << sqlStateText
				<< ", NativeError=" << nativeError
				<< ", Message=" << message
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