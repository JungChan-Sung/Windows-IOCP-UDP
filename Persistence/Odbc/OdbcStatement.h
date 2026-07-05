#pragma once

#include <Windows.h>
#include <sql.h>
#include <sqlext.h>

#include <expected>
#include <string_view>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>

namespace persistence::odbc
{
	class OdbcStatement final
	{
	public:
		using ExecuteResult = std::expected<void, core::DatabaseError>;
		using FetchResult = std::expected<bool, core::DatabaseError>;
		using ReadInt32Result = std::expected<int, core::DatabaseError>;

	private:
		SQLHSTMT statementHandle_ = SQL_NULL_HSTMT;

	public:
		OdbcStatement() = default;
		~OdbcStatement() noexcept;

		OdbcStatement(const OdbcStatement&) = delete;
		OdbcStatement& operator=(const OdbcStatement&) = delete;

		OdbcStatement(OdbcStatement&&) = delete;
		OdbcStatement& operator=(OdbcStatement&&) = delete;

	public:
		[[nodiscard]] ExecuteResult ExecuteDirect(const OdbcConnection& connection, std::string_view query);
		[[nodiscard]] FetchResult Fetch();
		[[nodiscard]] ReadInt32Result ReadInt32(SQLUSMALLINT columnNumber);

		void Close() noexcept;

	public:
		[[nodiscard]] SQLHSTMT GetHandle() const noexcept
		{
			return statementHandle_;
		}

		[[nodiscard]] bool IsOpen() const noexcept
		{
			return statementHandle_ != SQL_NULL_HSTMT;
		}
	};
}