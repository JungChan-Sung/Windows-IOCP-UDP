#pragma once

#include <string>

#include <Common/Time/TimeTypes.h>

#include <Server/Diagnostics/ServerStatusSnapshot.h>

namespace server::diagnostics
{
	class ServerStatusReporter
	{
	public:
		using Clock = common::time::Clock;
		using Duration = common::time::Duration;
		using TimePoint = common::time::TimePoint;

	private:
		static inline constexpr Duration defaultReportInterval = common::time::Seconds(10);

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
		[[nodiscard]] static std::string BuildMessage(const ServerStatusSnapshot& snapshot);

	public:
		void Reset() noexcept;

		[[nodiscard]] bool ShouldReport(TimePoint currentTime) noexcept;

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