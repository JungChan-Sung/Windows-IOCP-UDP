#pragma once

#include <Common/Log/AsyncLogWriter.h>

namespace common::log
{
	class AsyncLogWriterGuard final
	{
	private:
		AsyncLogWriter* logWriter_ = nullptr;

	public:
		explicit AsyncLogWriterGuard(AsyncLogWriter& logWriter) noexcept
			: logWriter_(&logWriter)
		{}

		~AsyncLogWriterGuard() noexcept
		{
			Reset();
		}

		AsyncLogWriterGuard(const AsyncLogWriterGuard&) = delete;
		AsyncLogWriterGuard& operator=(const AsyncLogWriterGuard&) = delete;

		AsyncLogWriterGuard(AsyncLogWriterGuard&&) = delete;
		AsyncLogWriterGuard& operator=(AsyncLogWriterGuard&&) = delete;

	public:
		void Reset() noexcept
		{
			if (logWriter_ == nullptr)
			{
				return;
			}

			logWriter_->Stop();
			logWriter_ = nullptr;
		}

	public:
		[[nodiscard]] bool IsActive() const noexcept
		{
			return logWriter_ != nullptr;
		}
	};
}