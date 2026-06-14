#pragma once

#include <chrono>
#include <mutex>
#include <random>

#include <Common/Time/TimeTypes.h>

#include "UdpFaultSimulationConfig.h"

namespace common::net
{
	class UdpFaultDecisionGenerator
	{
	public:
		using Config = UdpFaultSimulationConfig;

	public:
		struct Decision
		{
		public:
			bool shouldDrop = false;
			bool shouldDuplicate = false;
			bool shouldReorder = false;
			time::Duration delay{};
		};

	private:
		Config config_;
		std::mt19937 randomEngine_;
		std::mutex decisionMutex_;

	public:
		UdpFaultDecisionGenerator();
		explicit UdpFaultDecisionGenerator(const Config& config);
		~UdpFaultDecisionGenerator() noexcept = default;

		UdpFaultDecisionGenerator(const UdpFaultDecisionGenerator&) = delete;
		UdpFaultDecisionGenerator& operator=(const UdpFaultDecisionGenerator&) = delete;

		UdpFaultDecisionGenerator(UdpFaultDecisionGenerator&&) = delete;
		UdpFaultDecisionGenerator& operator=(UdpFaultDecisionGenerator&&) = delete;

	public:
		void SetConfig(const Config& config);
		void Reset();

		[[nodiscard]] Decision Generate();

	private:
		[[nodiscard]] bool Roll(float rate);
		[[nodiscard]] time::Duration GenerateDelay();
	};
}