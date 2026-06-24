#pragma once

#include <Common/Log/ILogger.h>
#include <Common/Log/LogRecord.h>

namespace common::log
{
	class NullLogger final : public ILogger
	{
	public:
		using ILogger::Log;

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
			static_cast<void>(logRecord);

			return true;
		}
	};
}