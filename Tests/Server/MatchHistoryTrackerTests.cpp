#include "MatchHistoryTrackerTests.h"

#include <chrono>
#include <cstdint>

#include <Common/Time/TimeTypes.h>

#include <Server/Game/KillEvent.h>
#include <Server/Game/MatchHistoryTracker.h>

#include <Tests/DebugTestResult.h>

namespace
{
	using SystemTimePoint = common::time::SystemTimePoint;

	[[nodiscard]] const server::game::MatchPlayerStats* FindPlayerStats(const server::game::CompletedMatch& completedMatch, std::int64_t persistentPlayerId) noexcept
	{
		for (const server::game::MatchPlayerStats& playerStats : completedMatch.playerStatsList)
		{
			if (playerStats.persistentPlayerId == persistentPlayerId)
			{
				return &playerStats;
			}
		}

		return nullptr;
	}

	void RunInitialStateTest(tests::DebugTestResult& result)
	{
		const server::game::MatchHistoryTracker tracker;

		tests::Expect(result, tracker.GetActiveMatchCount() == 0, "MatchHistoryTracker: initially no active match");
		tests::Expect(result, tracker.GetPendingCompletedMatchCount() == 0, "MatchHistoryTracker: initially no completed match");
		tests::Expect(result, !tracker.ContainsActiveMatch(1), "MatchHistoryTracker: initially room inactive");
		tests::Expect(result, tracker.GetActivePlayerCount(1) == 0, "MatchHistoryTracker: initially no active player");
	}

	void RunEnterAndLeaveLifecycleTest(tests::DebugTestResult& result)
	{
		server::game::MatchHistoryTracker tracker;

		const SystemTimePoint startTime = SystemTimePoint{} + std::chrono::seconds(10);
		const SystemTimePoint secondJoinTime = startTime + std::chrono::seconds(1);
		const SystemTimePoint firstLeaveTime = startTime + std::chrono::seconds(5);
		const SystemTimePoint secondLeaveTime = startTime + std::chrono::seconds(10);

		const bool firstEntered = tracker.EnterPlayer(1, 1001, startTime);
		const bool duplicateEntered = tracker.EnterPlayer(1, 1001, secondJoinTime);
		const bool secondEntered = tracker.EnterPlayer(1, 1002, secondJoinTime);

		tests::Expect(result, firstEntered, "MatchHistoryTracker: first player starts match");
		tests::Expect(result, !duplicateEntered, "MatchHistoryTracker: duplicate active player rejected");
		tests::Expect(result, secondEntered, "MatchHistoryTracker: second player joins match");
		tests::Expect(result, tracker.GetActiveMatchCount() == 1, "MatchHistoryTracker: one active match");
		tests::Expect(result, tracker.GetActivePlayerCount(1) == 2, "MatchHistoryTracker: two active players");

		const bool firstLeft = tracker.LeavePlayer(1, 1001, firstLeaveTime);

		tests::Expect(result, firstLeft, "MatchHistoryTracker: first player leaves");
		tests::Expect(result, tracker.ContainsActiveMatch(1), "MatchHistoryTracker: match remains while player active");
		tests::Expect(result, tracker.GetActivePlayerCount(1) == 1, "MatchHistoryTracker: one player remains");
		tests::Expect(result, tracker.GetPendingCompletedMatchCount() == 0, "MatchHistoryTracker: match not completed early");

		const bool secondLeft = tracker.LeavePlayer(1, 1002, secondLeaveTime);

		tests::Expect(result, secondLeft, "MatchHistoryTracker: last player leaves");
		tests::Expect(result, !tracker.ContainsActiveMatch(1), "MatchHistoryTracker: empty room match removed");
		tests::Expect(result, tracker.GetActiveMatchCount() == 0, "MatchHistoryTracker: no active match after last leave");
		tests::Expect(result, tracker.GetPendingCompletedMatchCount() == 1, "MatchHistoryTracker: completed match queued");

		server::game::CompletedMatchList completedMatchList = tracker.ExtractCompletedMatches();

		tests::Expect(result, completedMatchList.size() == 1, "MatchHistoryTracker: one completed match extracted");
		tests::Expect(result, tracker.GetPendingCompletedMatchCount() == 0, "MatchHistoryTracker: extraction clears completed queue");

		if (completedMatchList.size() != 1)
		{
			return;
		}

		const server::game::CompletedMatch& completedMatch = completedMatchList.front();

		tests::Expect(result, completedMatch.roomId == 1, "MatchHistoryTracker: completed room id");
		tests::Expect(result, completedMatch.startedAt == startTime, "MatchHistoryTracker: completed start time");
		tests::Expect(result, completedMatch.endedAt == secondLeaveTime, "MatchHistoryTracker: completed end time");
		tests::Expect(result, completedMatch.playerStatsList.size() == 2, "MatchHistoryTracker: completed participant count");

		const server::game::MatchPlayerStats* firstPlayerStats = FindPlayerStats(completedMatch, 1001);
		const server::game::MatchPlayerStats* secondPlayerStats = FindPlayerStats(completedMatch, 1002);

		tests::Expect(result, firstPlayerStats != nullptr, "MatchHistoryTracker: first participant retained");
		tests::Expect(result, secondPlayerStats != nullptr, "MatchHistoryTracker: second participant retained");

		if (firstPlayerStats != nullptr)
		{
			tests::Expect(result, firstPlayerStats->killCount == 0, "MatchHistoryTracker: first participant initial kills");
			tests::Expect(result, firstPlayerStats->deathCount == 0, "MatchHistoryTracker: first participant initial deaths");
		}

		if (secondPlayerStats != nullptr)
		{
			tests::Expect(result, secondPlayerStats->killCount == 0, "MatchHistoryTracker: second participant initial kills");
			tests::Expect(result, secondPlayerStats->deathCount == 0, "MatchHistoryTracker: second participant initial deaths");
		}
	}

	void RunKillRecordTest(tests::DebugTestResult& result)
	{
		server::game::MatchHistoryTracker tracker;

		const SystemTimePoint startTime = SystemTimePoint{} + std::chrono::seconds(20);

		static_cast<void>(tracker.EnterPlayer(1, 1001, startTime));
		static_cast<void>(tracker.EnterPlayer(1, 1002, startTime));

		const server::game::KillEvent killEvent{
			.killerPlayerId = 1,
			.killerPersistentPlayerId = 1001,
			.victimPlayerId = 2,
			.victimPersistentPlayerId = 1002,
			.roomId = 1,
		};

		const bool recorded = tracker.RecordKill(killEvent);

		tests::Expect(result, recorded, "MatchHistoryTracker: kill recorded");

		static_cast<void>(tracker.LeavePlayer(1, 1001, startTime + std::chrono::seconds(5)));
		static_cast<void>(tracker.LeavePlayer(1, 1002, startTime + std::chrono::seconds(6)));

		server::game::CompletedMatchList completedMatchList = tracker.ExtractCompletedMatches();

		tests::Expect(result, completedMatchList.size() == 1, "MatchHistoryTracker: kill test completed match");

		if (completedMatchList.size() != 1)
		{
			return;
		}

		const server::game::MatchPlayerStats* killerStats = FindPlayerStats(completedMatchList.front(), 1001);
		const server::game::MatchPlayerStats* victimStats = FindPlayerStats(completedMatchList.front(), 1002);

		tests::Expect(result, killerStats != nullptr, "MatchHistoryTracker: killer stats exist");
		tests::Expect(result, victimStats != nullptr, "MatchHistoryTracker: victim stats exist");

		if (killerStats != nullptr)
		{
			tests::Expect(result, killerStats->killCount == 1, "MatchHistoryTracker: killer count incremented");
			tests::Expect(result, killerStats->deathCount == 0, "MatchHistoryTracker: killer death unchanged");
		}

		if (victimStats != nullptr)
		{
			tests::Expect(result, victimStats->killCount == 0, "MatchHistoryTracker: victim kill unchanged");
			tests::Expect(result, victimStats->deathCount == 1, "MatchHistoryTracker: victim death incremented");
		}
	}

	void RunDepartedKillerStillRecordedTest(tests::DebugTestResult& result)
	{
		server::game::MatchHistoryTracker tracker;

		const SystemTimePoint startTime = SystemTimePoint{} + std::chrono::seconds(30);

		static_cast<void>(tracker.EnterPlayer(1, 1001, startTime));
		static_cast<void>(tracker.EnterPlayer(1, 1002, startTime));

		const bool killerLeft = tracker.LeavePlayer(1, 1001, startTime + std::chrono::seconds(1));

		tests::Expect(result, killerLeft, "MatchHistoryTracker: departed killer leaves");
		tests::Expect(result, tracker.ContainsActiveMatch(1), "MatchHistoryTracker: victim keeps match active");
		tests::Expect(result, tracker.GetActivePlayerCount(1) == 1, "MatchHistoryTracker: only victim remains active");

		const server::game::KillEvent delayedKillEvent{
			.killerPlayerId = 1,
			.killerPersistentPlayerId = 1001,
			.victimPlayerId = 2,
			.victimPersistentPlayerId = 1002,
			.roomId = 1,
		};

		const bool recorded = tracker.RecordKill(delayedKillEvent);

		tests::Expect(result, recorded, "MatchHistoryTracker: departed killer delayed bullet recorded");

		static_cast<void>(tracker.LeavePlayer(1, 1002, startTime + std::chrono::seconds(2)));

		server::game::CompletedMatchList completedMatchList = tracker.ExtractCompletedMatches();

		tests::Expect(result, completedMatchList.size() == 1, "MatchHistoryTracker: departed killer match completed");

		if (completedMatchList.size() != 1)
		{
			return;
		}

		const server::game::MatchPlayerStats* killerStats = FindPlayerStats(completedMatchList.front(), 1001);
		const server::game::MatchPlayerStats* victimStats = FindPlayerStats(completedMatchList.front(), 1002);

		tests::Expect(result, killerStats != nullptr && killerStats->killCount == 1, "MatchHistoryTracker: departed killer receives kill");
		tests::Expect(result, victimStats != nullptr && victimStats->deathCount == 1, "MatchHistoryTracker: delayed kill victim receives death");
	}

	void RunReenterPreservesStatsTest(tests::DebugTestResult& result)
	{
		server::game::MatchHistoryTracker tracker;

		const SystemTimePoint startTime = SystemTimePoint{} + std::chrono::seconds(40);

		static_cast<void>(tracker.EnterPlayer(1, 1001, startTime));
		static_cast<void>(tracker.EnterPlayer(1, 1002, startTime));

		const server::game::KillEvent killEvent{
			.killerPlayerId = 1,
			.killerPersistentPlayerId = 1001,
			.victimPlayerId = 2,
			.victimPersistentPlayerId = 1002,
			.roomId = 1,
		};

		static_cast<void>(tracker.RecordKill(killEvent));
		static_cast<void>(tracker.LeavePlayer(1, 1001, startTime + std::chrono::seconds(1)));

		const bool reentered = tracker.EnterPlayer(1, 1001, startTime + std::chrono::seconds(2));

		tests::Expect(result, reentered, "MatchHistoryTracker: departed participant can reenter active match");

		static_cast<void>(tracker.RecordKill(killEvent));
		static_cast<void>(tracker.LeavePlayer(1, 1001, startTime + std::chrono::seconds(3)));
		static_cast<void>(tracker.LeavePlayer(1, 1002, startTime + std::chrono::seconds(4)));

		server::game::CompletedMatchList completedMatchList = tracker.ExtractCompletedMatches();

		tests::Expect(result, completedMatchList.size() == 1, "MatchHistoryTracker: reenter match completed");

		if (completedMatchList.size() != 1)
		{
			return;
		}

		const server::game::MatchPlayerStats* firstPlayerStats = FindPlayerStats(completedMatchList.front(), 1001);
		const server::game::MatchPlayerStats* secondPlayerStats = FindPlayerStats(completedMatchList.front(), 1002);

		tests::Expect(result, firstPlayerStats != nullptr && firstPlayerStats->killCount == 2, "MatchHistoryTracker: reenter preserves previous kills");
		tests::Expect(result, secondPlayerStats != nullptr && secondPlayerStats->deathCount == 2, "MatchHistoryTracker: reenter preserves previous deaths");
	}

	void RunInvalidEventRejectedTest(tests::DebugTestResult& result)
	{
		server::game::MatchHistoryTracker tracker;

		const SystemTimePoint currentTime = SystemTimePoint{} + std::chrono::seconds(50);

		tests::Expect(result, !tracker.EnterPlayer(0, 1001, currentTime), "MatchHistoryTracker: invalid room enter rejected");
		tests::Expect(result, !tracker.EnterPlayer(1, 0, currentTime), "MatchHistoryTracker: invalid player enter rejected");
		tests::Expect(result, !tracker.LeavePlayer(1, 1001, currentTime), "MatchHistoryTracker: untracked leave rejected");

		static_cast<void>(tracker.EnterPlayer(1, 1001, currentTime));

		const server::game::KillEvent unknownVictimEvent{
			.killerPersistentPlayerId = 1001,
			.victimPersistentPlayerId = 9999,
			.roomId = 1,
		};

		const server::game::KillEvent unknownRoomEvent{
			.killerPersistentPlayerId = 1001,
			.victimPersistentPlayerId = 1001,
			.roomId = 2,
		};

		tests::Expect(result, !tracker.RecordKill(unknownVictimEvent), "MatchHistoryTracker: unknown victim rejected");
		tests::Expect(result, !tracker.RecordKill(unknownRoomEvent), "MatchHistoryTracker: unknown room kill rejected");
	}

	void RunRecordKillsTest(tests::DebugTestResult& result)
	{
		server::game::MatchHistoryTracker tracker;

		const SystemTimePoint startTime = SystemTimePoint{} + std::chrono::seconds(60);

		static_cast<void>(tracker.EnterPlayer(1, 1001, startTime));
		static_cast<void>(tracker.EnterPlayer(1, 1002, startTime));

		const server::game::KillEvent killEventList[]{
			server::game::KillEvent{
				.killerPersistentPlayerId = 1001,
				.victimPersistentPlayerId = 1002,
				.roomId = 1,
			},
			server::game::KillEvent{
				.killerPersistentPlayerId = 1001,
				.victimPersistentPlayerId = 9999,
				.roomId = 1,
			},
			server::game::KillEvent{
				.killerPersistentPlayerId = 1002,
				.victimPersistentPlayerId = 1001,
				.roomId = 1,
			},
		};

		const std::size_t recordedCount = tracker.RecordKills(killEventList);

		tests::Expect(result, recordedCount == 2, "MatchHistoryTracker: RecordKills returns successful count");

		static_cast<void>(tracker.LeavePlayer(1, 1001, startTime + std::chrono::seconds(1)));
		static_cast<void>(tracker.LeavePlayer(1, 1002, startTime + std::chrono::seconds(2)));

		server::game::CompletedMatchList completedMatchList = tracker.ExtractCompletedMatches();

		if (completedMatchList.size() != 1)
		{
			tests::Expect(result, false, "MatchHistoryTracker: RecordKills completed match missing");
			return;
		}

		const server::game::MatchPlayerStats* firstPlayerStats = FindPlayerStats(completedMatchList.front(), 1001);
		const server::game::MatchPlayerStats* secondPlayerStats = FindPlayerStats(completedMatchList.front(), 1002);

		tests::Expect(result, firstPlayerStats != nullptr && firstPlayerStats->killCount == 1 && firstPlayerStats->deathCount == 1,
			"MatchHistoryTracker: RecordKills first stats");
		tests::Expect(result, secondPlayerStats != nullptr && secondPlayerStats->killCount == 1 && secondPlayerStats->deathCount == 1,
			"MatchHistoryTracker: RecordKills second stats");
	}

	void RunCompleteAllTest(tests::DebugTestResult& result)
	{
		server::game::MatchHistoryTracker tracker;

		const SystemTimePoint startTime = SystemTimePoint{} + std::chrono::seconds(70);
		const SystemTimePoint endTime = startTime + std::chrono::seconds(10);

		static_cast<void>(tracker.EnterPlayer(1, 1001, startTime));
		static_cast<void>(tracker.EnterPlayer(2, 2001, startTime + std::chrono::seconds(1)));

		tracker.CompleteAll(endTime);

		tests::Expect(result, tracker.GetActiveMatchCount() == 0, "MatchHistoryTracker: CompleteAll clears active matches");
		tests::Expect(result, tracker.GetPendingCompletedMatchCount() == 2, "MatchHistoryTracker: CompleteAll queues every match");

		const server::game::CompletedMatchList completedMatchList = tracker.ExtractCompletedMatches();

		tests::Expect(result, completedMatchList.size() == 2, "MatchHistoryTracker: CompleteAll extracts every match");

		for (const server::game::CompletedMatch& completedMatch : completedMatchList)
		{
			tests::Expect(result, completedMatch.endedAt == endTime, "MatchHistoryTracker: CompleteAll end time");
		}
	}

	void RunClearTest(tests::DebugTestResult& result)
	{
		server::game::MatchHistoryTracker tracker;

		const SystemTimePoint currentTime = SystemTimePoint{} + std::chrono::seconds(80);

		static_cast<void>(tracker.EnterPlayer(1, 1001, currentTime));
		static_cast<void>(tracker.EnterPlayer(2, 2001, currentTime));
		static_cast<void>(tracker.LeavePlayer(2, 2001, currentTime + std::chrono::seconds(1)));

		tracker.Clear();

		tests::Expect(result, tracker.GetActiveMatchCount() == 0, "MatchHistoryTracker: clear active matches");
		tests::Expect(result, tracker.GetPendingCompletedMatchCount() == 0, "MatchHistoryTracker: clear completed matches");
	}
}

namespace tests::server
{
	DebugTestResult RunMatchHistoryTrackerTests()
	{
		DebugTestResult result{};

		RunInitialStateTest(result);
		RunEnterAndLeaveLifecycleTest(result);
		RunKillRecordTest(result);
		RunDepartedKillerStillRecordedTest(result);
		RunReenterPreservesStatsTest(result);
		RunInvalidEventRejectedTest(result);
		RunRecordKillsTest(result);
		RunCompleteAllTest(result);
		RunClearTest(result);

		return result;
	}
}