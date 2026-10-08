#pragma once

#include <vector>

#include <Server/Config/ServerConfig.h>
#include <Server/Config/ServerConfigWarning.h>

namespace server::config
{
	// ServerConfig의 최종 유효성을 검증하고 자동값 및 상호 의존 설정을 실제 사용 값으로 정규화하는 클래스
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