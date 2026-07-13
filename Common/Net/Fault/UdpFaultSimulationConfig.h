#pragma once

#include <chrono>
#include <cstdint>

#include <Common/Time/TimeTypes.h>

namespace common::net
{
	struct UdpFaultSimulationConfig
	{
	public:
		bool enabled = false;

		float dropRate = 0.0F;
		float duplicateRate = 0.0F;
		float reorderRate = 0.0F;

		time::Milliseconds minDelay{};
		time::Milliseconds maxDelay{};
		time::Milliseconds reorderDelay = std::chrono::milliseconds(100);

		std::uint32_t randomSeed = 5489;
	};
}