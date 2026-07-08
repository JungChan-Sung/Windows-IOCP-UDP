#pragma once

#include <Windows.h>
#include <sql.h>
#include <sqlext.h>

#include <cstdint>
#include <deque>
#include <expected>
#include <string>
#include <string_view>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>

namespace persistence::odbc
{
	class OdbcStatement final
	{
	private:
		struct BoundStringParameter
		{
		public:
			std::string value;
			SQLLEN indicator = SQL_NTS;
		};

		struct BoundInt64Parameter
		{
		public:
			SQLBIGINT value = 0;
			SQLLEN indicator = 0;
		};

	public:
		using ExecuteResult = std::expected<void, core::DatabaseError>;
		using BindResult = std::expected<void, core::DatabaseError>;
		using FetchResult = std::expected<bool, core::DatabaseError>;
		using ReadInt32Result = std::expected<int, core::DatabaseError>;
		using ReadInt64Result = std::expected<std::int64_t, core::DatabaseError>;
		using ReadStringResult = std::expected<std::string, core::DatabaseError>;

	private:
		SQLHSTMT statementHandle_ = SQL_NULL_HSTMT;
		std::deque<BoundStringParameter> boundStringParameters_;
		std::deque<BoundInt64Parameter> boundInt64Parameters_;

	public:
		OdbcStatement() = default;
		~OdbcStatement() noexcept;

		OdbcStatement(const OdbcStatement&) = delete;
		OdbcStatement& operator=(const OdbcStatement&) = delete;

		OdbcStatement(OdbcStatement&&) = delete;
		OdbcStatement& operator=(OdbcStatement&&) = delete;

	public:
		[[nodiscard]] ExecuteResult ExecuteDirect(const OdbcConnection& connection, std::string_view query);

		[[nodiscard]] ExecuteResult Prepare(const OdbcConnection& connection, std::string_view query);
		[[nodiscard]] BindResult BindInputString(SQLUSMALLINT parameterNumber, std::string_view value);
		[[nodiscard]] BindResult BindInputInt64(SQLUSMALLINT parameterNumber, std::int64_t value);
		[[nodiscard]] ExecuteResult Execute();

		[[nodiscard]] FetchResult Fetch();
		[[nodiscard]] ReadInt32Result ReadInt32(SQLUSMALLINT columnNumber);
		[[nodiscard]] ReadInt64Result ReadInt64(SQLUSMALLINT columnNumber);
		[[nodiscard]] ReadStringResult ReadString(SQLUSMALLINT columnNumber);

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