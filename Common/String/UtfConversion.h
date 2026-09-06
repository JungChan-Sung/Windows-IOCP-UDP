#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace common::string
{
	// 프로젝트 내부 UTF-8 문자열과 Windows Wide API의 UTF-16 문자열 간 변환 오류를 표현
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