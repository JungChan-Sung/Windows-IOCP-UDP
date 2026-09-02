#include "ServerConfigDatabaseApplier.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Common/Config/ConfigText.h>

#include <Server/Config/ServerConfigLoader.h>

namespace server::config
{
	ServerConfigDatabaseApplyResult ServerConfigDatabaseApplier::Apply(ServerConfig& serverConfig, std::span<const persistence::config::ServerConfigEntry> entryList)
	{
		ServerConfigDatabaseApplyResult result{};

		for (const persistence::config::ServerConfigEntry& entry : entryList)
		{
			const std::string_view fullKey = common::config::Trim(entry.key);
			const std::size_t separatorPosition = fullKey.rfind('.');
			if (separatorPosition == std::string_view::npos || separatorPosition == 0 || separatorPosition + 1 >= fullKey.size())
			{
				ServerConfigWarning warning{};
				warning.message = "Invalid database config key ignored. Key=" + entry.key;
				result.warningList.push_back(std::move(warning));
				continue;
			}

			const std::string_view section = common::config::Trim(fullKey.substr(0, separatorPosition));
			const std::string_view key = common::config::Trim(fullKey.substr(separatorPosition + 1));
			const std::string normalizedSection = common::config::ToLowerCopy(section);

			if (normalizedSection == "network" || normalizedSection == "database")
			{
				ServerConfigWarning warning{};
				warning.message = "Database config cannot override bootstrap setting. Key=" + entry.key;
				result.warningList.push_back(std::move(warning));
				continue;
			}

			std::vector<ServerConfigWarning> applyWarningList = ServerConfigLoader::ApplySingleValue(serverConfig, section, key, entry.value);
			if (applyWarningList.empty())
			{
				++result.appliedCount;
				continue;
			}

			for (ServerConfigWarning& warning : applyWarningList)
			{
				warning.message = "Database config: " + warning.message;
				result.warningList.push_back(std::move(warning));
			}
		}

		return result;
	}
}