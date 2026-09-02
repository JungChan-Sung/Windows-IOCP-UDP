#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <Persistence/Config/ServerConfigRepository.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Config/ServerConfigWarning.h>

namespace server::config
{
	struct ServerConfigDatabaseApplyResult
	{
	public:
		std::size_t appliedCount = 0;
		std::vector<ServerConfigWarning> warningList;
	};

	class ServerConfigDatabaseApplier final
	{
	public:
		ServerConfigDatabaseApplier() = delete;
		~ServerConfigDatabaseApplier() = delete;

		ServerConfigDatabaseApplier(const ServerConfigDatabaseApplier&) = delete;
		ServerConfigDatabaseApplier& operator=(const ServerConfigDatabaseApplier&) = delete;

		ServerConfigDatabaseApplier(ServerConfigDatabaseApplier&&) = delete;
		ServerConfigDatabaseApplier& operator=(ServerConfigDatabaseApplier&&) = delete;

	public:
		[[nodiscard]] static ServerConfigDatabaseApplyResult Apply(
			ServerConfig& serverConfig,
			std::span<const persistence::config::ServerConfigEntry> entryList
		);
	};
}