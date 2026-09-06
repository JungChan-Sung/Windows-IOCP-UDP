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
		// INI와 DB 설정이 동일한 타입 변환·key 해석 규칙을 사용하도록 
		// 기존 ServerConfigLoader의 단일 값 적용 경로를 재사용하는 함수
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

