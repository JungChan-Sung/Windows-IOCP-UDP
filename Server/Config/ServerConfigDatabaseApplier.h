#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <Persistence/Config/ServerConfigRepository.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Config/ServerConfigWarning.h>

namespace server::config
{
	// DB의 문자열 설정을 기존 Config parsing 규칙으로 해석해 
	// bootstrap 설정을 제외한 ServerConfig 항목에 적용하는 클래스
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