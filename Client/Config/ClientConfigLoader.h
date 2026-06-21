#pragma once

#include <filesystem>
#include <vector>

#include <Client/Config/ClientConfig.h>
#include <Client/Config/ClientConfigWarning.h>

namespace client::config
{
	struct ClientConfigLoadResult
	{
	public:
		ClientConfig config{};
		bool loadedFromFile = false;
		std::vector<ClientConfigWarning> warningList;
	};

	class ClientConfigLoader
	{
	public:
		ClientConfigLoader() = delete;
		~ClientConfigLoader() = delete;

		ClientConfigLoader(const ClientConfigLoader&) = delete;
		ClientConfigLoader& operator=(const ClientConfigLoader&) = delete;

		ClientConfigLoader(ClientConfigLoader&&) = delete;
		ClientConfigLoader& operator=(ClientConfigLoader&&) = delete;

	public:
		[[nodiscard]] static ClientConfigLoadResult Load(const std::filesystem::path& filePath);
		[[nodiscard]] static ClientConfigLoadResult LoadValidated(const std::filesystem::path& filePath);
		[[nodiscard]] static ClientConfig LoadOrDefault(const std::filesystem::path& filePath);
	};
}