#pragma once

#include <mutex>
#include <vector>

#include <Common/Log/ILogger.h>
#include <Common/Log/LogRecord.h>

namespace tests::log
{
	class MemoryLogger final : public common::log::ILogger
	{
	private:
		mutable std::mutex mutex_;
		mutable std::vector<common::log::LogRecord> logRecordList_;

	public:
		MemoryLogger() = default;
		~MemoryLogger() noexcept override = default;

		MemoryLogger(const MemoryLogger&) = delete;
		MemoryLogger& operator=(const MemoryLogger&) = delete;

		MemoryLogger(MemoryLogger&&) = delete;
		MemoryLogger& operator=(MemoryLogger&&) = delete;

	public:
		using common::log::ILogger::Log;

		bool Log(const common::log::LogRecord& logRecord) const override
		{
			std::scoped_lock lock(mutex_);

			logRecordList_.push_back(logRecord);

			return true;
		}

	public:
		[[nodiscard]] std::vector<common::log::LogRecord> GetLogRecords() const
		{
			std::scoped_lock lock(mutex_);

			return logRecordList_;
		}

		[[nodiscard]] std::size_t GetLogRecordCount() const
		{
			std::scoped_lock lock(mutex_);

			return logRecordList_.size();
		}
	};
}