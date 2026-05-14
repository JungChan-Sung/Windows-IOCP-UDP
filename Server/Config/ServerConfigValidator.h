#pragma once

#include <vector>

#include <Server/Config/ServerConfig.h>
#include <Server/Config/ServerConfigWarning.h>

namespace server::config
{
	class ServerConfigValidator
	{
	public:
		ServerConfigValidator() = delete;
		~ServerConfigValidator() = delete;

		ServerConfigValidator(const ServerConfigValidator&) = delete;
		ServerConfigValidator& operator=(const ServerConfigValidator&) = delete;

		ServerConfigValidator(ServerConfigValidator&&) = delete;
		ServerConfigValidator& operator=(ServerConfigValidator&&) = delete;

	public:
		[[nodiscard]] static std::vector<ServerConfigWarning> ValidateAndNormalize(ServerConfig& serverConfig);
	};
}