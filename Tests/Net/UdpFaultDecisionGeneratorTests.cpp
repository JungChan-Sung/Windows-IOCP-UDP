#include "UdpFaultDecisionGeneratorTests.h"

#include <array>
#include <chrono>

#include <Common/Net/UdpFaultDecisionGenerator.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using Config = common::net::UdpFaultSimulationConfig;
	using Decision = common::net::UdpFaultDecisionGenerator::Decision;
	using Generator = common::net::UdpFaultDecisionGenerator;

	[[nodiscard]] bool IsSameDecision(const Decision& left, const Decision& right) noexcept
	{
		return left.shouldDrop == right.shouldDrop
			&& left.shouldDuplicate == right.shouldDuplicate
			&& left.shouldReorder == right.shouldReorder
			&& left.delay == right.delay;
	}

	void RunDisabledSimulationTest(tests::DebugTestResult& result)
	{
		Generator generator;
		const Decision decision = generator.Generate();

		tests::Expect(result, !decision.shouldDrop, "UdpFaultDecisionGenerator: disabled does not drop");
		tests::Expect(result, !decision.shouldDuplicate, "UdpFaultDecisionGenerator: disabled does not duplicate");
		tests::Expect(result, !decision.shouldReorder, "UdpFaultDecisionGenerator: disabled does not reorder");
		tests::Expect(result, decision.delay == common::time::Milliseconds::zero(),
			"UdpFaultDecisionGenerator: disabled has no delay");
	}

	void RunGuaranteedDropTest(tests::DebugTestResult& result)
	{
		Config config{};
		config.enabled = true;
		config.dropRate = 1.0F;
		config.duplicateRate = 1.0F;
		config.reorderRate = 1.0F;
		config.minDelay = common::time::Milliseconds(50);
		config.maxDelay = common::time::Milliseconds(50);

		Generator generator(config);
		const Decision decision = generator.Generate();

		tests::Expect(result, decision.shouldDrop, "UdpFaultDecisionGenerator: guaranteed drop");
		tests::Expect(result, !decision.shouldDuplicate, "UdpFaultDecisionGenerator: dropped packet not duplicated");
		tests::Expect(result, !decision.shouldReorder, "UdpFaultDecisionGenerator: dropped packet not reordered");
		tests::Expect(result, decision.delay == common::time::Milliseconds::zero(),
			"UdpFaultDecisionGenerator: dropped packet has no delay");
	}

	void RunGuaranteedFaultsTest(tests::DebugTestResult& result)
	{
		Config config{};
		config.enabled = true;
		config.dropRate = 0.0F;
		config.duplicateRate = 1.0F;
		config.reorderRate = 1.0F;
		config.minDelay = common::time::Milliseconds(25);
		config.maxDelay = common::time::Milliseconds(25);
		config.reorderDelay = common::time::Milliseconds(75);

		Generator generator(config);
		const Decision decision = generator.Generate();

		tests::Expect(result, !decision.shouldDrop, "UdpFaultDecisionGenerator: zero drop rate");
		tests::Expect(result, decision.shouldDuplicate, "UdpFaultDecisionGenerator: guaranteed duplicate");
		tests::Expect(result, decision.shouldReorder, "UdpFaultDecisionGenerator: guaranteed reorder");
		tests::Expect(result, decision.delay == common::time::Milliseconds(100),
			"UdpFaultDecisionGenerator: base and reorder delay combined");
	}

	void RunSameSeedProducesSameSequenceTest(tests::DebugTestResult& result)
	{
		Config config{};
		config.enabled = true;
		config.dropRate = 0.25F;
		config.duplicateRate = 0.35F;
		config.reorderRate = 0.45F;
		config.minDelay = common::time::Milliseconds(10);
		config.maxDelay = common::time::Milliseconds(50);
		config.randomSeed = 12345;

		Generator firstGenerator(config);
		Generator secondGenerator(config);

		for (int index = 0; index < 32; ++index)
		{
			tests::Expect(
				result,
				IsSameDecision(firstGenerator.Generate(), secondGenerator.Generate()),
				"UdpFaultDecisionGenerator: same seed produces same sequence"
			);
		}
	}

	void RunResetReplaysSequenceTest(tests::DebugTestResult& result)
	{
		Config config{};
		config.enabled = true;
		config.dropRate = 0.2F;
		config.duplicateRate = 0.3F;
		config.reorderRate = 0.4F;
		config.minDelay = common::time::Milliseconds(5);
		config.maxDelay = common::time::Milliseconds(40);
		config.randomSeed = 777;

		Generator generator(config);
		std::array<Decision, 16> originalDecisionList{};

		for (Decision& decision : originalDecisionList)
		{
			decision = generator.Generate();
		}

		generator.Reset();

		for (const Decision& originalDecision : originalDecisionList)
		{
			tests::Expect(
				result,
				IsSameDecision(originalDecision, generator.Generate()),
				"UdpFaultDecisionGenerator: reset replays sequence"
			);
		}
	}

	void RunRandomDelayRangeTest(tests::DebugTestResult& result)
	{
		Config config{};
		config.enabled = true;
		config.minDelay = common::time::Milliseconds(10);
		config.maxDelay = common::time::Milliseconds(50);
		config.randomSeed = 12345;

		Generator generator(config);

		for (int index = 0; index < 100; ++index)
		{
			const Decision decision = generator.Generate();

			tests::Expect(
				result,
				decision.delay >= common::time::Milliseconds(10)
				&& decision.delay <= common::time::Milliseconds(50),
				"UdpFaultDecisionGenerator: random delay stays within range"
			);
		}
	}
}

namespace tests::net
{
	tests::DebugTestResult RunUdpFaultDecisionGeneratorTests()
	{
		tests::DebugTestResult result{};

		RunDisabledSimulationTest(result);
		RunGuaranteedDropTest(result);
		RunGuaranteedFaultsTest(result);
		RunSameSeedProducesSameSequenceTest(result);
		RunResetReplaysSequenceTest(result);
		RunRandomDelayRangeTest(result);

		return result;
	}
}