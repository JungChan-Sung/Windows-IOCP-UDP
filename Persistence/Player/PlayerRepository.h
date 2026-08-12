#pragma once

#include <cstdint>
#include <expected>
#include <optional>

#include <Common/Identity/IdentityTypes.h>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>

namespace persistence::player
{
	struct PlayerRecord
	{
	public:
		common::identity::PersistentPlayerId playerId = 0;
		common::identity::AccountId accountId = 0;
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
		[[nodiscard]] CreatePlayerResult CreatePlayer(common::identity::AccountId accountId);
		[[nodiscard]] FindPlayerResult FindPlayerByAccountId(common::identity::AccountId accountId);
	};
}