#pragma once

#include <chrono>
#include <string>

#include <Server/Diagnostics/ServerStatusSnapshot.h>

namespace server::diagnostics
{
	class ServerStatusReporter
	{
	public:
		using Clock = std::chrono::steady_clock;
		using Duration = Clock::duration;
		using TimePoint = Clock::time_point;

	private:
		static inline constexpr Duration defaultReportInterval = std::chrono::seconds(10);

	private:
		Duration reportInterval_ = defaultReportInterval;
		TimePoint nextReportTime_;
		bool isEnabled_ = true;
		bool isFirstCheck_ = true;

	public:
		ServerStatusReporter() = default;
		~ServerStatusReporter() noexcept = default;

		ServerStatusReporter(const ServerStatusReporter&) = delete;
		ServerStatusReporter& operator=(const ServerStatusReporter&) = delete;

		ServerStatusReporter(ServerStatusReporter&&) = delete;
		ServerStatusReporter& operator=(ServerStatusReporter&&) = delete;

	public:
		void Reset() noexcept;

		[[nodiscard]] bool ShouldReport(TimePoint currentTime) noexcept;
		[[nodiscard]] std::string BuildMessage(const ServerStatusSnapshot& snapshot) const;

	public:
		void SetEnabled(bool isEnabled) noexcept
		{
			isEnabled_ = isEnabled;
		}

		[[nodiscard]] bool IsEnabled() const noexcept
		{
			return isEnabled_;
		}

		void SetReportInterval(Duration reportInterval) noexcept
		{
			reportInterval_ = reportInterval;
		}

		[[nodiscard]] Duration GetReportInterval() const noexcept
		{
			return reportInterval_;
		}
	};
}