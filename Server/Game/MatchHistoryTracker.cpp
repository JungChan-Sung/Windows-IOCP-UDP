#include "MatchHistoryTracker.h"

#include <algorithm>
#include <utility>

namespace server::game
{
	CompletedMatch MatchHistoryTracker::BuildCompletedMatch(ActiveMatch&& activeMatch, SystemTimePoint endedAt)
	{
		std::vector<MatchPlayerStats> playerStatsList;
		playerStatsList.reserve(activeMatch.playerStatsTable.size());

		for (auto& [_, playerStats] : activeMatch.playerStatsTable)
		{
			playerStatsList.push_back(std::move(playerStats));
		}

		std::ranges::sort(playerStatsList, {}, &MatchPlayerStats::persistentPlayerId);

		return CompletedMatch{
			.roomId = activeMatch.roomId,
			.startedAt = activeMatch.startedAt,
			.endedAt = endedAt,
			.playerStatsList = std::move(playerStatsList),
		};
	}

	bool MatchHistoryTracker::EnterPlayer(RoomId roomId, std::int64_t persistentPlayerId, SystemTimePoint currentTime)
	{
		if (roomId <= 0 || persistentPlayerId <= 0)
		{
			return false;
		}

		auto [matchIterator, matchInserted] = activeMatchTable_.try_emplace(roomId);
		ActiveMatch& activeMatch = matchIterator->second;

		if (matchInserted)
		{
			activeMatch.roomId = roomId;
			activeMatch.startedAt = currentTime;
		}

		const auto [_, activePlayerInserted] = activeMatch.activePlayerSet.insert(persistentPlayerId);
		if (!activePlayerInserted)
		{
			return false;
		}

		activeMatch.playerStatsTable.try_emplace(
			persistentPlayerId,
			MatchPlayerStats{
				.persistentPlayerId = persistentPlayerId,
			}
			);

		return true;
	}

	bool MatchHistoryTracker::LeavePlayer(RoomId roomId, std::int64_t persistentPlayerId, SystemTimePoint currentTime)
	{
		if (roomId <= 0 || persistentPlayerId <= 0)
		{
			return false;
		}

		const auto matchIterator = activeMatchTable_.find(roomId);
		if (matchIterator == activeMatchTable_.end())
		{
			return false;
		}

		ActiveMatch& activeMatch = matchIterator->second;
		const std::size_t erasedCount = activeMatch.activePlayerSet.erase(persistentPlayerId);
		if (erasedCount == 0)
		{
			return false;
		}

		if (!activeMatch.activePlayerSet.empty())
		{
			return true;
		}

		completedMatchList_.push_back(BuildCompletedMatch(std::move(activeMatch), currentTime));
		activeMatchTable_.erase(matchIterator);

		return true;
	}

	bool MatchHistoryTracker::RecordKill(const KillEvent& killEvent)
	{
		if (killEvent.roomId <= 0 || killEvent.killerPersistentPlayerId <= 0 || killEvent.victimPersistentPlayerId <= 0)
		{
			return false;
		}

		const auto matchIterator = activeMatchTable_.find(killEvent.roomId);
		if (matchIterator == activeMatchTable_.end())
		{
			return false;
		}

		ActiveMatch& activeMatch = matchIterator->second;

		const auto killerIterator = activeMatch.playerStatsTable.find(killEvent.killerPersistentPlayerId);
		if (killerIterator == activeMatch.playerStatsTable.end())
		{
			return false;
		}

		const auto victimIterator = activeMatch.playerStatsTable.find(killEvent.victimPersistentPlayerId);
		if (victimIterator == activeMatch.playerStatsTable.end())
		{
			return false;
		}

		++killerIterator->second.killCount;
		++victimIterator->second.deathCount;

		return true;
	}

	std::size_t MatchHistoryTracker::RecordKills(std::span<const KillEvent> killEventList)
	{
		std::size_t recordedCount = 0;

		for (const KillEvent& killEvent : killEventList)
		{
			if (RecordKill(killEvent))
			{
				++recordedCount;
			}
		}

		return recordedCount;
	}

	void MatchHistoryTracker::CompleteAll(SystemTimePoint currentTime)
	{
		completedMatchList_.reserve(completedMatchList_.size() + activeMatchTable_.size());

		for (auto& [_, activeMatch] : activeMatchTable_)
		{
			completedMatchList_.push_back(BuildCompletedMatch(std::move(activeMatch), currentTime));
		}

		activeMatchTable_.clear();
	}

	CompletedMatchList MatchHistoryTracker::ExtractCompletedMatches()
	{
		CompletedMatchList completedMatchList = std::move(completedMatchList_);
		completedMatchList_.clear();
		return completedMatchList;
	}

	void MatchHistoryTracker::Clear() noexcept
	{
		activeMatchTable_.clear();
		completedMatchList_.clear();
	}

	bool MatchHistoryTracker::ContainsActiveMatch(RoomId roomId) const noexcept
	{
		return activeMatchTable_.contains(roomId);
	}

	std::size_t MatchHistoryTracker::GetActivePlayerCount(RoomId roomId) const noexcept
	{
		const auto matchIterator = activeMatchTable_.find(roomId);
		if (matchIterator == activeMatchTable_.end())
		{
			return 0;
		}

		return matchIterator->second.activePlayerSet.size();
	}
}