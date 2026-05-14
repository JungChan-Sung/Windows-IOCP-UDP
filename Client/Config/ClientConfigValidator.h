#pragma once

#include <vector>

#include <Client/Config/ClientConfig.h>
#include <Client/Config/ClientConfigWarning.h>

namespace client::config
{
	class ClientConfigValidator
	{
	public:
		ClientConfigValidator() = delete;
		~ClientConfigValidator() = delete;

		ClientConfigValidator(const ClientConfigValidator&) = delete;
		ClientConfigValidator& operator=(const ClientConfigValidator&) = delete;

		ClientConfigValidator(ClientConfigValidator&&) = delete;
		ClientConfigValidator& operator=(ClientConfigValidator&&) = delete;

	public:
		[[nodiscard]] static std::vector<ClientConfigWarning> ValidateAndNormalize(ClientConfig& clientConfig);
	};
}