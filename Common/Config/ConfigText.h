#pragma once

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <Common/Time/TimeTypes.h>

namespace common::config
{
	[[nodiscard]] inline std::string_view Trim(std::string_view text) noexcept
	{
		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
		{
			text.remove_prefix(1);
		}

		while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
		{
			text.remove_suffix(1);
		}

		return text;
	}

	[[nodiscard]] inline std::string_view RemoveComment(std::string_view text) noexcept
	{
		const std::string_view trimmedText = Trim(text);
		if (!trimmedText.empty() && trimmedText.front() == ';')
		{
			return {};
		}

		const std::size_t commentPosition = text.find('#');
		if (commentPosition == std::string_view::npos)
		{
			return text;
		}

		return text.substr(0, commentPosition);
	}

	[[nodiscard]] inline std::string ToLowerCopy(std::string_view text)
	{
		std::string result(text);
		std::ranges::transform(
			result,
			result.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			}
		);

		return result;
	}

	[[nodiscard]] inline std::optional<unsigned long long> TryParseUnsigned(std::string_view text) noexcept
	{
		text = Trim(text);
		if (text.empty())
		{
			return std::nullopt;
		}

		unsigned long long value = 0;
		const char* begin = text.data();
		const char* end = text.data() + text.size();

		const auto [position, errorCode] = std::from_chars(begin, end, value);
		if (errorCode != std::errc{} || position != end)
		{
			return std::nullopt;
		}

		return value;
	}

	[[nodiscard]] inline std::optional<long long> TryParseSigned(std::string_view text) noexcept
	{
		text = Trim(text);
		if (text.empty())
		{
			return std::nullopt;
		}

		long long value = 0;
		const char* begin = text.data();
		const char* end = text.data() + text.size();

		const auto [position, errorCode] = std::from_chars(begin, end, value);
		if (errorCode != std::errc{} || position != end)
		{
			return std::nullopt;
		}

		return value;
	}

	[[nodiscard]] inline std::optional<time::Milliseconds> TryParseMilliseconds(std::string_view text) noexcept
	{
		const std::optional<unsigned long long> parsedValue = TryParseUnsigned(text);
		if (!parsedValue.has_value())
		{
			return std::nullopt;
		}

		using MillisecondsRep = time::Milliseconds::rep;
		if (*parsedValue > static_cast<unsigned long long>(std::numeric_limits<MillisecondsRep>::max()))
		{
			return std::nullopt;
		}

		return time::Milliseconds(static_cast<MillisecondsRep>(*parsedValue));
	}

	[[nodiscard]] inline std::optional<float> TryParseFloat(std::string_view text)
	{
		text = Trim(text);
		if (text.empty())
		{
			return std::nullopt;
		}

		try
		{
			std::string valueText(text);

			std::size_t processedCount = 0;
			const float value = std::stof(valueText, &processedCount);

			if (processedCount != valueText.size())
			{
				return std::nullopt;
			}

			return value;
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	[[nodiscard]] inline std::optional<bool> TryParseBool(std::string_view text)
	{
		const std::string normalizedText = ToLowerCopy(Trim(text));

		if (normalizedText == "true" || normalizedText == "1" || normalizedText == "yes" || normalizedText == "on")
		{
			return true;
		}

		if (normalizedText == "false" || normalizedText == "0" || normalizedText == "no" || normalizedText == "off")
		{
			return false;
		}

		return std::nullopt;
	}
}