#include "UtfConversion.h"

#include <Windows.h>

#include <limits>

namespace common::string
{
	static UtfConversionError MakeConversionError(UtfConversionFailure invalidTextFailure) noexcept
	{
		const DWORD nativeError = ::GetLastError();

		return UtfConversionError{
			.failure = (nativeError == ERROR_NO_UNICODE_TRANSLATION) ? invalidTextFailure : UtfConversionFailure::NativeConversionFailed,
			.nativeError = static_cast<std::uint32_t>(nativeError),
		};
	}

	Utf16ConversionResult ConvertUtf8ToUtf16(std::string_view text)
	{
		if (text.empty())
		{
			return std::wstring{};
		}

		if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
		{
			return std::unexpected(UtfConversionError{
					.failure = UtfConversionFailure::InputTooLarge,
				}
				);
		}

		const int sourceLength = static_cast<int>(text.size());
		const int requiredLength = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), sourceLength, nullptr, 0);
		if (requiredLength <= 0)
		{
			return std::unexpected(MakeConversionError(UtfConversionFailure::InvalidUtf8));
		}

		std::wstring convertedText(static_cast<std::size_t>(requiredLength), L'\0');
		const int convertedLength = ::MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			text.data(),
			sourceLength,
			convertedText.data(),
			requiredLength
		);
		if (convertedLength != requiredLength)
		{
			return std::unexpected(MakeConversionError(UtfConversionFailure::InvalidUtf8));
		}

		return convertedText;
	}

	Utf8ConversionResult ConvertUtf16ToUtf8(std::wstring_view text)
	{
		if (text.empty())
		{
			return std::string{};
		}

		if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
		{
			return std::unexpected(UtfConversionError{
					.failure = UtfConversionFailure::InputTooLarge,
				}
				);
		}

		const int sourceLength = static_cast<int>(text.size());
		const int requiredLength = ::WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			text.data(),
			sourceLength,
			nullptr,
			0,
			nullptr,
			nullptr
		);
		if (requiredLength <= 0)
		{
			return std::unexpected(MakeConversionError(UtfConversionFailure::InvalidUtf16));
		}

		std::string convertedText(static_cast<std::size_t>(requiredLength), '\0');
		const int convertedLength = ::WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			text.data(),
			sourceLength,
			convertedText.data(),
			requiredLength,
			nullptr,
			nullptr
		);
		if (convertedLength != requiredLength)
		{
			return std::unexpected(MakeConversionError(UtfConversionFailure::InvalidUtf16));
		}

		return convertedText;
	}
}