#pragma once

#include <Windows.h>
#include <sql.h>
#include <sqlext.h>

#include <expected>

#include <Persistence/Core/DatabaseError.h>

namespace persistence::odbc
{
	// ODBC Environment Handle의 초기화와 수명을 RAII 방식으로 관리하는 클래스
	class OdbcEnvironment final
	{
	public:
		using InitializeResult = std::expected<void, core::DatabaseError>;

	private:
		SQLHENV environmentHandle_ = SQL_NULL_HENV;

	public:
		OdbcEnvironment() = default;
		~OdbcEnvironment() noexcept;

		OdbcEnvironment(const OdbcEnvironment&) = delete;
		OdbcEnvironment& operator=(const OdbcEnvironment&) = delete;

		OdbcEnvironment(OdbcEnvironment&&) = delete;
		OdbcEnvironment& operator=(OdbcEnvironment&&) = delete;

	public:
		[[nodiscard]] InitializeResult Initialize();
		void Close() noexcept;

	public:
		[[nodiscard]] SQLHENV GetHandle() const noexcept
		{
			return environmentHandle_;
		}

		[[nodiscard]] bool IsInitialized() const noexcept
		{
			return environmentHandle_ != SQL_NULL_HENV;
		}
	};
}