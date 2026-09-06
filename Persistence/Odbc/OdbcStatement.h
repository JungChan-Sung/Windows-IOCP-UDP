#pragma once

#include <Windows.h>
#include <sql.h>
#include <sqlext.h>

#include <cstdint>
#include <deque>
#include <expected>
#include <string>
#include <string_view>

#include <Common/Time/TimeTypes.h>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>

namespace persistence::odbc
{
	// ODBC Statement Handle과 바인딩된 Parameter 버퍼의 수명을 함께 관리하는 클래스
	class OdbcStatement final
	{
	private:
		struct BoundStringParameter
		{
		public:
			std::wstring value;
			SQLLEN indicator = SQL_NTS;
		};

		struct BoundInt64Parameter
		{
		public:
			SQLBIGINT value = 0;
			SQLLEN indicator = 0;
		};

		struct BoundTimestampParameter
		{
		public:
			SQL_TIMESTAMP_STRUCT value{};
			SQLLEN indicator = sizeof(SQL_TIMESTAMP_STRUCT);
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
		// SQLBindParameter가 실행 시점까지 참조하는 버퍼 주소를 유지하기 위해
		// 바인딩된 값의 수명과 주소 안정성을 Statement가 보장
		std::deque<BoundStringParameter> boundStringParameters_;
		std::deque<BoundInt64Parameter> boundInt64Parameters_;
		std::deque<BoundTimestampParameter> boundTimestampParameters_;

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
		[[nodiscard]] BindResult BindInputSystemTimePoint(SQLUSMALLINT parameterNumber, common::time::SystemTimePoint value);
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