#include "UdpFaultDecisionGenerator.h"

#include <algorithm>
#include <utility>

namespace
{
	[[nodiscard]] common::net::UdpFaultSimulationConfig NormalizeConfig(common::net::UdpFaultSimulationConfig config) noexcept
	{
		config.dropRate = std::clamp(config.dropRate, 0.0F, 1.0F);
		config.duplicateRate = std::clamp(config.duplicateRate, 0.0F, 1.0F);
		config.reorderRate = std::clamp(config.reorderRate, 0.0F, 1.0F);

		config.minDelay = std::max(config.minDelay, std::chrono::milliseconds::zero());
		config.maxDelay = std::max(config.maxDelay, std::chrono::milliseconds::zero());
		config.reorderDelay = std::max(config.reorderDelay, std::chrono::milliseconds::zero());

		if (config.minDelay > config.maxDelay)
		{
			std::swap(config.minDelay, config.maxDelay);
		}

		return config;
	}
}

namespace common::net
{
	UdpFaultDecisionGenerator::UdpFaultDecisionGenerator()
		: randomEngine_(config_.randomSeed)
	{}

	UdpFaultDecisionGenerator::UdpFaultDecisionGenerator(const Config& config)
		: config_(NormalizeConfig(config)),
		randomEngine_(config_.randomSeed)
	{}

	void UdpFaultDecisionGenerator::SetConfig(const Config& config)
	{
		std::scoped_lock lock(decisionMutex_);

		config_ = NormalizeConfig(config);
		randomEngine_.seed(config_.randomSeed);
	}

	void UdpFaultDecisionGenerator::Reset()
	{
		std::scoped_lock lock(decisionMutex_);
		randomEngine_.seed(config_.randomSeed);
	}

	UdpFaultDecisionGenerator::Decision UdpFaultDecisionGenerator::Generate()
	{
		std::scoped_lock lock(decisionMutex_);

		Decision decision{};

		if (!config_.enabled)
		{
			return decision;
		}

		decision.shouldDrop = Roll(config_.dropRate);
		if (decision.shouldDrop)
		{
			return decision;
		}

		decision.shouldDuplicate = Roll(config_.duplicateRate);
		decision.shouldReorder = Roll(config_.reorderRate);
		decision.delay = GenerateDelay();

		if (decision.shouldReorder)
		{
			decision.delay += config_.reorderDelay;
		}

		return decision;
	}

	bool UdpFaultDecisionGenerator::Roll(float rate)
	{
		if (rate <= 0.0F)
		{
			return false;
		}

		if (rate >= 1.0F)
		{
			return true;
		}

		std::uniform_real_distribution<float> distribution(0.0F, 1.0F);
		return distribution(randomEngine_) < rate;
	}

	time::Duration UdpFaultDecisionGenerator::GenerateDelay()
	{
		if (config_.minDelay == config_.maxDelay)
		{
			return config_.minDelay;
		}

		std::uniform_int_distribution<time::Milliseconds::rep> distribution(config_.minDelay.count(), config_.maxDelay.count());
		return std::chrono::duration_cast<time::Duration>(time::Milliseconds(distribution(randomEngine_)));
	}
}