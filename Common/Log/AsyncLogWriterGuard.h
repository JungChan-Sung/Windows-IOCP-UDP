#pragma once

#include <Common/Log/AsyncLogWriter.h>

namespace common::log
{
	// Scope 종료 시 AsyncLogWriter를 Drain 종료해 모든 return 경로에서 정리를 보장하는 클래스
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