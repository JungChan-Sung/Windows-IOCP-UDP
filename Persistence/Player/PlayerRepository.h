#pragma once

#include <cstdint>
#include <expected>
#include <optional>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>

namespace persistence::player
{
	struct PlayerRecord
	{
	public:
		std::int64_t playerId = 0;
		std::int64_t accountId = 0;
	};

	class PlayerRepository final
	{
	public:
		using CreatePlayerResult = std::expected<PlayerRecord, core::DatabaseError>;
		using FindPlayerResult = std::expected<std::optional<PlayerRecord>, core::DatabaseError>;

	private:
		odbc::OdbcConnection& connection_;

	public:
		explicit PlayerRepository(odbc::OdbcConnection& connection) noexcept;
		~PlayerRepository() noexcept = default;

		PlayerRepository(const PlayerRepository&) = delete;
		PlayerRepository& operator=(const PlayerRepository&) = delete;

		PlayerRepository(PlayerRepository&&) = delete;
		PlayerRepository& operator=(PlayerRepository&&) = delete;

	public:
		[[nodiscard]] CreatePlayerResult CreatePlayer(std::int64_t accountId);
		[[nodiscard]] FindPlayerResult FindPlayerByAccountId(std::int64_t accountId);
	};
}