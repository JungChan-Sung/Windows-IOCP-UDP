#pragma once

#include <expected>
#include <string_view>

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

	private:
		odbc::OdbcEnvironment environment_;
		odbc::OdbcConnection connection_;
		bool enabled_ = false;

	public:
		PersistenceRuntime() = default;
		~PersistenceRuntime() noexcept = default;

		PersistenceRuntime(const PersistenceRuntime&) = delete;
		PersistenceRuntime& operator=(const PersistenceRuntime&) = delete;

		PersistenceRuntime(PersistenceRuntime&&) = delete;
		PersistenceRuntime& operator=(PersistenceRuntime&&) = delete;

	public:
		[[nodiscard]] StartResult Start(const PersistenceRuntimeStartConfig& startConfig);
		void Stop() noexcept;

	public:
		[[nodiscard]] bool IsEnabled() const noexcept
		{
			return enabled_;
		}

		[[nodiscard]] bool IsStarted() const noexcept
		{
			return connection_.IsOpen();
		}
	};
}