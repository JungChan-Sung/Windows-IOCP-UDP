#pragma once

#include <string_view>

#include <Common/Log/ILogger.h>
#include <Common/Log/LogLevel.h>

namespace common::log
{
	class NullLogger final : public ILogger
	{
	public:
		NullLogger() = default;
		~NullLogger() noexcept override = default;

		NullLogger(const NullLogger&) = delete;
		NullLogger& operator=(const NullLogger&) = delete;

		NullLogger(NullLogger&&) = delete;
		NullLogger& operator=(NullLogger&&) = delete;

	public:
		bool Log(const LogRecord& logRecord) const override
		{
			return true;
		}
	};
}