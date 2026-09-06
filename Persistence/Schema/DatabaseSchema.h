#pragma once

#include <expected>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>

namespace persistence::schema
{
	// 서버 실행에 필요한 기본 테이블과 제약조건이 존재하도록 DB 초기 스키마를 구성하는 클래스
	class DatabaseSchema
	{
	public:
		using InitializeResult = std::expected<void, core::DatabaseError>;

	public:
		DatabaseSchema() = delete;
		~DatabaseSchema() = delete;

		DatabaseSchema(const DatabaseSchema&) = delete;
		DatabaseSchema& operator=(const DatabaseSchema&) = delete;

		DatabaseSchema(DatabaseSchema&&) = delete;
		DatabaseSchema& operator=(DatabaseSchema&&) = delete;

	public:
		// 이미 존재하는 테이블은 유지하고 누락된 테이블만 생성해 반복 초기화를 허용하는 함수
		[[nodiscard]] static InitializeResult Initialize(odbc::OdbcConnection& connection);
	};
}