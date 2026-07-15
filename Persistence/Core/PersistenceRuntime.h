#pragma once

#include <expected>
#include <mutex>
#include <string_view>

#include <Persistence/Account/AccountRepository.h>
#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>
#include <Persistence/Odbc/OdbcEnvironment.h>

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

	public:
		[[nodiscard]] StartResult Start(const PersistenceRuntimeStartConfig& startConfig);
		void Stop() noexcept;

		[[nodiscard]] CreateAccountResult CreateAccount(const account::AccountCreateRequest& request);
		[[nodiscard]] FindAccountResult FindAccountByLoginName(std::string_view loginName);
		[[nodiscard]] ExistsAccountResult ExistsByLoginName(std::string_view loginName);

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