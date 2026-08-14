#pragma once

#include <cstdint>
#include <expected>
#include <span>

#include <Common/Game/GameTypes.h>
#include <Common/Identity/IdentityTypes.h>
#include <Common/Time/TimeTypes.h>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>

namespace persistence::match
{
	struct MatchPlayerCreateRecord
	{
	public:
		common::identity::PersistentPlayerId persistentPlayerId = 0;
		std::uint32_t killCount = 0;
		std::uint32_t deathCount = 0;
	};

	struct MatchCreateRequest
	{
	public:
		common::game::RoomId roomId = 0;
		common::time::SystemTimePoint startedAt{};
		common::time::SystemTimePoint endedAt{};
		std::span<const MatchPlayerCreateRecord> playerStatsList;
	};

	class MatchHistoryRepository final
	{
	public:
		using MatchId = common::identity::MatchId;
		using SaveMatchResult = std::expected<MatchId, core::DatabaseError>;

	private:
		odbc::OdbcConnection& connection_;

	public:
		explicit MatchHistoryRepository(odbc::OdbcConnection& connection) noexcept;
		~MatchHistoryRepository() noexcept = default;

		MatchHistoryRepository(const MatchHistoryRepository&) = delete;
		MatchHistoryRepository& operator=(const MatchHistoryRepository&) = delete;

		MatchHistoryRepository(MatchHistoryRepository&&) = delete;
		MatchHistoryRepository& operator=(MatchHistoryRepository&&) = delete;

	public:
		[[nodiscard]] SaveMatchResult SaveMatch(const MatchCreateRequest& request);
	};
}