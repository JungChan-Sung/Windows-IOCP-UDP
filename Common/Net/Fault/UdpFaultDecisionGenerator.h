#pragma once

#include <mutex>
#include <random>

#include <Common/Time/TimeTypes.h>

#include "UdpFaultSimulationConfig.h"

namespace common::net
{
	// 설정된 확률과 지연 범위에 따라 패킷별 Fault 적용 여부를 결정하는 클래스
	class UdpFaultDecisionGenerator
	{
	public:
		struct Decision
		{
		public:
			bool shouldDrop = false;
			bool shouldDuplicate = false;
			bool shouldReorder = false;
			time::Duration delay{};
		};

	public:
		using Config = UdpFaultSimulationConfig;

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