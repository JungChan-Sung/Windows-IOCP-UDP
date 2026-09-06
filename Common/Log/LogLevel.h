#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

namespace common::log
{
	// 선언 순서를 로그 심각도 순서로 유지하며 minimum level 필터링에 사용
	enum class LogLevel
	{
		Trace,
		Debug,
		Info,
		Warning,
		Error
	};

	[[nodiscard]] constexpr std::string_view ToString(LogLevel logLevel) noexcept
	{
		switch (logLevel)
		{
		case LogLevel::Trace:
			return "Trace";

		case LogLevel::Debug:
			return "Debug";

		case LogLevel::Info:
			return "Info";

		case LogLevel::Warning:
			return "Warning";

		case LogLevel::Error:
			return "Error";

		default:
			return "Unknown";
		}
	}

	[[nodiscard]] inline bool EqualsIgnoreCase(std::string_view left, std::string_view right) noexcept
	{
		return left.size() == right.size()
			&& std::ranges::equal(
				left,
				right,
				[](unsigned char leftCharacter, unsigned char rightCharacter)
				{
					return std::tolower(leftCharacter) == std::tolower(rightCharacter);
				}
			);
	}

	[[nodiscard]] inline std::string_view TrimLogLevelText(std::string_view text) noexcept
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

	[[nodiscard]] inline std::optional<LogLevel> TryParseLogLevel(std::string_view text) noexcept
	{
		text = TrimLogLevelText(text);

		if (EqualsIgnoreCase(text, "trace"))
		{
			return LogLevel::Trace;
		}

		if (EqualsIgnoreCase(text, "debug"))
		{
			return LogLevel::Debug;
		}

		if (EqualsIgnoreCase(text, "info"))
		{
			return LogLevel::Info;
		}

		if (EqualsIgnoreCase(text, "warning") || EqualsIgnoreCase(text, "warn"))
		{
			return LogLevel::Warning;
		}

		if (EqualsIgnoreCase(text, "error"))
		{
			return LogLevel::Error;
		}

		return std::nullopt;
	}
}