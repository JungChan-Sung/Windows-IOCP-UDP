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

	// ODBC Connection Handle과 Transaction 상태의 수명을 관리하는 클래스
	class OdbcConnection final
	{
	public:
		using OpenResult = std::expected<void, core::DatabaseError>;
		using HealthCheckResult = std::expected<void, core::DatabaseError>;
		using TransactionResult = std::expected<void, core::DatabaseError>;

	private:
		SQLHDBC connectionHandle_ = SQL_NULL_HDBC;
		bool transactionActive_ = false;

	public:
		OdbcConnection() = default;
		~OdbcConnection() noexcept;

		OdbcConnection(const OdbcConnection&) = delete;
		OdbcConnection& operator=(const OdbcConnection&) = delete;

		OdbcConnection(OdbcConnection&&) = delete;
		OdbcConnection& operator=(OdbcConnection&&) = delete;

	public:
		[[nodiscard]] OpenResult Open(const OdbcEnvironment& environment, const OdbcConnectionOpenConfig& openConfig);
		// 명시적으로 완료되지 않은 Transaction은 연결 종료 시 Rollback하는 함수
		void Close() noexcept;

		[[nodiscard]] HealthCheckResult ExecuteHealthCheck() const;

		[[nodiscard]] TransactionResult BeginTransaction();
		[[nodiscard]] TransactionResult CommitTransaction();
		[[nodiscard]] TransactionResult RollbackTransaction();

	public:
		[[nodiscard]] bool IsTransactionActive() const noexcept
		{
			return transactionActive_;
		}

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