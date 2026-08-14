#pragma once

#include <cstdint>
#include <expected>
#include <mutex>
#include <string_view>

#include <Common/Identity/IdentityTypes.h>

#include <Persistence/Account/AccountRepository.h>
#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Match/MatchHistoryRepository.h>
#include <Persistence/Odbc/OdbcConnection.h>
#include <Persistence/Odbc/OdbcEnvironment.h>
#include <Persistence/Player/PlayerRepository.h>

namespace persistence
{
	struct PersistenceRuntimeStartConfig
	{
	public:
		bool enabled = false;
		std::string_view connectionString;
		int connectionTimeoutSeconds = 5;
	};

	class PersistenceRuntime final
	{
	public:
		using StartResult = std::expected<void, core::DatabaseError>;

		using CreateAccountResult = account::AccountRepository::CreateAccountResult;
		using FindAccountResult = account::AccountRepository::FindAccountResult;
		using ExistsAccountResult = account::AccountRepository::ExistsResult;

		using CreatePlayerResult = player::PlayerRepository::CreatePlayerResult;
		using FindPlayerResult = player::PlayerRepository::FindPlayerResult;
		using FindOrCreatePlayerResult = std::expected<player::PlayerRecord, core::DatabaseError>;

		using SaveMatchResult = match::MatchHistoryRepository::SaveMatchResult;

	private:
		inline static constexpr std::int32_t duplicateIndexNativeError = 2601;
		inline static constexpr std::int32_t uniqueConstraintNativeError = 2627;

	private:
		odbc::OdbcEnvironment environment_;
		odbc::OdbcConnection connection_;

		mutable std::mutex databaseMutex_;

		bool enabled_ = false;

	public:
		PersistenceRuntime() = default;
		~PersistenceRuntime() noexcept = default;

		PersistenceRuntime(const PersistenceRuntime&) = delete;
		PersistenceRuntime& operator=(const PersistenceRuntime&) = delete;

		PersistenceRuntime(PersistenceRuntime&&) = delete;
		PersistenceRuntime& operator=(PersistenceRuntime&&) = delete;

	private:
		[[nodiscard]] static core::DatabaseError MakeNotStartedError();
		[[nodiscard]] static bool IsDuplicateConstraintError(const core::DatabaseError& databaseError) noexcept;

	public:
		[[nodiscard]] StartResult Start(const PersistenceRuntimeStartConfig& startConfig);
		void Stop() noexcept;

		[[nodiscard]] CreateAccountResult CreateAccount(const account::AccountCreateRequest& request);
		[[nodiscard]] FindAccountResult FindAccountByLoginName(std::string_view loginName);
		[[nodiscard]] ExistsAccountResult ExistsByLoginName(std::string_view loginName);

		[[nodiscard]] CreatePlayerResult CreatePlayer(common::identity::AccountId accountId);
		[[nodiscard]] FindPlayerResult FindPlayerByAccountId(common::identity::AccountId accountId);
		[[nodiscard]] FindOrCreatePlayerResult FindOrCreatePlayerByAccountId(common::identity::AccountId accountId);

		[[nodiscard]] SaveMatchResult SaveMatch(const match::MatchCreateRequest& request);

	private:
		void StopUnlocked() noexcept;

	public:
		[[nodiscard]] bool IsEnabled() const;
		[[nodiscard]] bool IsStarted() const;

	private:
		[[nodiscard]] bool IsStartedUnlocked() const noexcept
		{
			return connection_.IsOpen();
		}
	};
}