#include "PacketReplayGuardTests.h"

#include <cstdint>
#include <limits>

#include <Common/Net/Auth/PacketReplayGuard.h>

namespace
{
	void RunFirstSequenceAcceptedTest(tests::DebugTestResult& result)
	{
		common::net::PacketReplayGuard replayGuard;

		tests::Expect(
			result,
			replayGuard.TryAccept(100),
			"PacketReplayGuard: first sequence accepted"
		);
	}

	void RunDuplicateSequenceRejectedTest(tests::DebugTestResult& result)
	{
		common::net::PacketReplayGuard replayGuard;

		static_cast<void>(replayGuard.TryAccept(100));

		tests::Expect(
			result,
			!replayGuard.TryAccept(100),
			"PacketReplayGuard: duplicate sequence rejected"
		);
	}

	void RunNewerSequenceAcceptedTest(tests::DebugTestResult& result)
	{
		common::net::PacketReplayGuard replayGuard;

		static_cast<void>(replayGuard.TryAccept(100));

		tests::Expect(
			result,
			replayGuard.TryAccept(101),
			"PacketReplayGuard: newer sequence accepted"
		);

		tests::Expect(
			result,
			replayGuard.TryAccept(102),
			"PacketReplayGuard: consecutive newer sequence accepted"
		);
	}

	void RunOutOfOrderSequenceAcceptedOnceTest(tests::DebugTestResult& result)
	{
		common::net::PacketReplayGuard replayGuard;

		static_cast<void>(replayGuard.TryAccept(100));
		static_cast<void>(replayGuard.TryAccept(102));

		tests::Expect(
			result,
			replayGuard.TryAccept(101),
			"PacketReplayGuard: out-of-order sequence accepted"
		);

		tests::Expect(
			result,
			!replayGuard.TryAccept(101),
			"PacketReplayGuard: repeated out-of-order sequence rejected"
		);
	}

	void RunOldSequenceOutsideWindowRejectedTest(tests::DebugTestResult& result)
	{
		common::net::PacketReplayGuard replayGuard;

		static_cast<void>(replayGuard.TryAccept(100));
		static_cast<void>(replayGuard.TryAccept(356));

		tests::Expect(
			result,
			!replayGuard.TryAccept(100),
			"PacketReplayGuard: sequence outside replay window rejected"
		);
	}

	void RunLargeForwardJumpAcceptedTest(tests::DebugTestResult& result)
	{
		common::net::PacketReplayGuard replayGuard;

		static_cast<void>(replayGuard.TryAccept(100));

		tests::Expect(
			result,
			replayGuard.TryAccept(1000),
			"PacketReplayGuard: large forward sequence jump accepted"
		);

		tests::Expect(
			result,
			!replayGuard.TryAccept(100),
			"PacketReplayGuard: history before large jump rejected"
		);
	}

	void RunWrappedSequenceAcceptedTest(tests::DebugTestResult& result)
	{
		common::net::PacketReplayGuard replayGuard;

		constexpr std::uint32_t maxSequence =
			std::numeric_limits<std::uint32_t>::max();

		static_cast<void>(replayGuard.TryAccept(maxSequence - 1));

		tests::Expect(
			result,
			replayGuard.TryAccept(maxSequence),
			"PacketReplayGuard: maximum sequence accepted"
		);

		tests::Expect(
			result,
			replayGuard.TryAccept(0),
			"PacketReplayGuard: wrapped zero sequence accepted"
		);

		tests::Expect(
			result,
			replayGuard.TryAccept(1),
			"PacketReplayGuard: sequence after wrap accepted"
		);

		tests::Expect(
			result,
			!replayGuard.TryAccept(maxSequence),
			"PacketReplayGuard: duplicate sequence before wrap rejected"
		);
	}

	void RunResetClearsReplayHistoryTest(tests::DebugTestResult& result)
	{
		common::net::PacketReplayGuard replayGuard;

		static_cast<void>(replayGuard.TryAccept(100));

		tests::Expect(
			result,
			!replayGuard.TryAccept(100),
			"PacketReplayGuard: duplicate rejected before reset"
		);

		replayGuard.Reset();

		tests::Expect(
			result,
			replayGuard.TryAccept(100),
			"PacketReplayGuard: reset clears replay history"
		);
	}
}

namespace tests::net
{
	DebugTestResult RunPacketReplayGuardTests()
	{
		DebugTestResult result{};

		RunFirstSequenceAcceptedTest(result);
		RunDuplicateSequenceRejectedTest(result);
		RunNewerSequenceAcceptedTest(result);
		RunOutOfOrderSequenceAcceptedOnceTest(result);
		RunOldSequenceOutsideWindowRejectedTest(result);
		RunLargeForwardJumpAcceptedTest(result);
		RunWrappedSequenceAcceptedTest(result);
		RunResetClearsReplayHistoryTest(result);

		return result;
	}
}