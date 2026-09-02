#pragma once

#include <expected>
#include <string>
#include <vector>

#include <Persistence/Core/DatabaseError.h>
#include <Persistence/Odbc/OdbcConnection.h>

namespace persistence::config
{
	struct ServerConfigEntry
	{
	public:
		std::string key;
		std::string value;
	};

	class ServerConfigRepository final
	{
	public:
		using EntryList = std::vector<ServerConfigEntry>;
		using LoadAllResult = std::expected<EntryList, core::DatabaseError>;

	private:
		odbc::OdbcConnection& connection_;

	public:
		explicit ServerConfigRepository(odbc::OdbcConnection& connection) noexcept;
		~ServerConfigRepository() noexcept = default;

		ServerConfigRepository(const ServerConfigRepository&) = delete;
		ServerConfigRepository& operator=(const ServerConfigRepository&) = delete;

		ServerConfigRepository(ServerConfigRepository&&) = delete;
		ServerConfigRepository& operator=(ServerConfigRepository&&) = delete;

	public:
		[[nodiscard]] LoadAllResult LoadAll();
	};
}