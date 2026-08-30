#pragma once

#include <cstddef>
#include <cstdint>

#include <Common/Time/TimeTypes.h>

namespace client::game
{
	class ServerTickTimeline
	{
	public:
		struct SampleTimeResult
		{
			common::time::TimePoint sampleTime{};
			bool wasReanchored = false;
		};

	public:
		using TimePoint = common::time::TimePoint;
		using Milliseconds = common::time::Milliseconds;

	private:
		static inline constexpr Milliseconds reanchorDriftThreshold = Milliseconds(100);
		static inline constexpr std::size_t reanchorConfirmationCount = 4;

	private:
		Milliseconds tickInterval_;
		TimePoint anchorTime_;

		std::uint32_t anchorServerTick_ = 0;
		std::size_t reanchorCandidateCount_ = 0;

		bool isInitialized_ = false;

	public:
		ServerTickTimeline();
		~ServerTickTimeline() noexcept = default;

		ServerTickTimeline(const ServerTickTimeline&) = delete;
		ServerTickTimeline& operator=(const ServerTickTimeline&) = delete;

		ServerTickTimeline(ServerTickTimeline&&) = delete;
		ServerTickTimeline& operator=(ServerTickTimeline&&) = delete;

	private:
		[[nodiscard]] static Milliseconds ResolveTickInterval(Milliseconds tickInterval) noexcept;

	public:
		void Clear() noexcept;

		[[nodiscard]] SampleTimeResult ResolveSampleTime(std::uint32_t serverTick, Milliseconds tickInterval, TimePoint arrivalTime) noexcept;
	};
}