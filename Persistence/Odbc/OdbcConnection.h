#pragma once

#include <Windows.h>
#include <sql.h>
#include <sqlext.h>

#include <expected>
#include <string_view>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcEnvironment.h>

namespace persistence::odbc
{
	struct OdbcConnectionOpenConfig
	{
	public:
		std::string_view connectionString;
		int connectionTimeoutSeconds = 5;
	};

	class OdbcConnection final
	{
	public:
		using OpenResult = std::expected<void, core::DatabaseError>;
		using HealthCheckResult = std::expected<void, core::DatabaseError>;

	private:
		SQLHDBC connectionHandle_ = SQL_NULL_HDBC;

	public:
		OdbcConnection() = default;
		~OdbcConnection() noexcept;

		OdbcConnection(const OdbcConnection&) = delete;
		OdbcConnection& operator=(const OdbcConnection&) = delete;

		OdbcConnection(OdbcConnection&&) = delete;
		OdbcConnection& operator=(OdbcConnection&&) = delete;

	public:
		[[nodiscard]] OpenResult Open(const OdbcEnvironment& environment, const OdbcConnectionOpenConfig& openConfig);
		void Close() noexcept;

		[[nodiscard]] HealthCheckResult ExecuteHealthCheck() const;

	public:
		[[nodiscard]] SQLHDBC GetHandle() const noexcept
		{
			return connectionHandle_;
		}

		[[nodiscard]] bool IsOpen() const noexcept
		{
			return connectionHandle_ != SQL_NULL_HDBC;
		}
	};
}