#pragma once

namespace common::log
{
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
}