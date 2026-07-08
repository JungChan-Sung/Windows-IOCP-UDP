#pragma once

#include <expected>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>

namespace persistence::schema
{
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
		[[nodiscard]] static InitializeResult Initialize(odbc::OdbcConnection& connection);
	};
}