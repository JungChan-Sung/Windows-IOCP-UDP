#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include <Server/Config/ServerConfig.h>
#include <Server/Config/ServerConfigWarning.h>

namespace server::config
{
	struct ServerConfigLoadResult
	{
	public:
		ServerConfig config{};
		bool loadedFromFile = false;
		std::vector<ServerConfigWarning> warningList;
	};

	class ServerConfigLoader
	{
	public:
		ServerConfigLoader() = delete;
		~ServerConfigLoader() = delete;

		ServerConfigLoader(const ServerConfigLoader&) = delete;
		ServerConfigLoader& operator=(const ServerConfigLoader&) = delete;

		ServerConfigLoader(ServerConfigLoader&&) = delete;
		ServerConfigLoader& operator=(ServerConfigLoader&&) = delete;

	public:
		[[nodiscard]] static std::vector<ServerConfigWarning> ApplySingleValue(
			ServerConfig& serverConfig,
			std::string_view section,
			std::string_view key,
			std::string_view value
		);

		[[nodiscard]] static ServerConfigLoadResult Load(const std::filesystem::path& filePath);
		[[nodiscard]] static ServerConfigLoadResult LoadValidated(const std::filesystem::path& filePath);
		[[nodiscard]] static ServerConfig LoadOrDefault(const std::filesystem::path& filePath);
	};
}

