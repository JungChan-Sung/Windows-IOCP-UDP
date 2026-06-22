#pragma once

#include <sstream>
#include <string>
#include <utility>

namespace common::log
{
	class LogMessageBuilder
	{
	private:
		std::ostringstream stream_;

	public:
		LogMessageBuilder() = default;
		~LogMessageBuilder() = default;

		LogMessageBuilder(const LogMessageBuilder&) = delete;
		LogMessageBuilder& operator=(const LogMessageBuilder&) = delete;

		LogMessageBuilder(LogMessageBuilder&&) = default;
		LogMessageBuilder& operator=(LogMessageBuilder&&) = default;

	public:
		template <typename Value>
		LogMessageBuilder& Append(Value&& value)
		{
			stream_ << std::forward<Value>(value);
			return *this;
		}

	public:
		[[nodiscard]] std::string Build() const
		{
			return stream_.str();
		}
	};
}