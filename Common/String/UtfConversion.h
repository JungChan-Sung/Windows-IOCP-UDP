#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace common::string
{
	enum class UtfConversionFailure
	{
		InputTooLarge,
		InvalidUtf8,
		InvalidUtf16,
		NativeConversionFailed,
	};

	struct UtfConversionError
	{
	public:
		UtfConversionFailure failure = UtfConversionFailure::NativeConversionFailed;
		unsigned long nativeError = 0;
	};

	using Utf16ConversionResult = std::expected<std::wstring, UtfConversionError>;
	using Utf8ConversionResult = std::expected<std::string, UtfConversionError>;

	[[nodiscard]] Utf16ConversionResult ConvertUtf8ToUtf16(std::string_view text);
	[[nodiscard]] Utf8ConversionResult ConvertUtf16ToUtf8(std::wstring_view text);
}