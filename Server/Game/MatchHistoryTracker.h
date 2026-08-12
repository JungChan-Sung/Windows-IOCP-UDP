#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Identity/IdentityTypes.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Game/KillEvent.h>

namespace server::game
{
	struct MatchPlayerStats
	{
	public:
		common::identity::PersistentPlayerId persistentPlayerId = 0;
		std::uint32_t killCount = 0;
		std::uint32_t deathCount = 0;
	};

	struct CompletedMatch
	{
	public:
		common::game::RoomId roomId = 0;
		common::time::SystemTimePoint startedAt{};
		common::time::SystemTimePoint endedAt{};
		std::vector<MatchPlayerStats> playerStatsList;
	};

	using CompletedMatchList = std::vector<CompletedMatch>;

	class MatchHistoryTracker final
	{
	public:
		using RoomId = common::game::RoomId;
		using PersistentPlayerId = common::identity::PersistentPlayerId;
		using SystemTimePoint = common::time::SystemTimePoint;

	private:
		struct ActiveMatch
		{
		public:
			RoomId roomId = 0;
			SystemTimePoint startedAt{};
			std::unordered_map<PersistentPlayerId, MatchPlayerStats> playerStatsTable;
			std::unordered_set<PersistentPlayerId> activePlayerSet;
		};

	private:
		using ActiveMatchTable = std::unordered_map<RoomId, ActiveMatch>;

	private:
		ActiveMatchTable activeMatchTable_;
		CompletedMatchList completedMatchList_;

	public:
		MatchHistoryTracker() = default;
		~MatchHistoryTracker() noexcept = default;

		MatchHistoryTracker(const MatchHistoryTracker&) = delete;
		MatchHistoryTracker& operator=(const MatchHistoryTracker&) = delete;

		MatchHistoryTracker(MatchHistoryTracker&&) = delete;
		MatchHistoryTracker& operator=(MatchHistoryTracker&&) = delete;

	private:
		[[nodiscard]] static CompletedMatch BuildCompletedMatch(ActiveMatch&& activeMatch, SystemTimePoint endedAt);

	public:
		[[nodiscard]] bool EnterPlayer(RoomId roomId, std::int64_t persistentPlayerId, SystemTimePoint currentTime);
		[[nodiscard]] bool LeavePlayer(RoomId roomId, std::int64_t persistentPlayerId, SystemTimePoint currentTime);

		[[nodiscard]] bool RecordKill(const KillEvent& killEvent);
		[[nodiscard]] std::size_t RecordKills(std::span<const KillEvent> killEventList);

		void CompleteAll(SystemTimePoint currentTime);

		[[nodiscard]] CompletedMatchList ExtractCompletedMatches();

		void Clear() noexcept;

	public:
		[[nodiscard]] bool ContainsActiveMatch(RoomId roomId) const noexcept;
		[[nodiscard]] std::size_t GetActivePlayerCount(RoomId roomId) const noexcept;

		[[nodiscard]] std::size_t GetActiveMatchCount() const noexcept
		{
			return activeMatchTable_.size();
		}

		[[nodiscard]] std::size_t GetPendingCompletedMatchCount() const noexcept
		{
			return completedMatchList_.size();
		}
	};
}