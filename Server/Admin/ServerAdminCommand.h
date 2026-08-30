#pragma once

#include <string_view>

namespace server::admin
{
	enum class ServerAdminCommandType
	{
		Unknown,
		Help,
		Status,
		Stop,
	};

	struct ServerAdminCommand
	{
		ServerAdminCommandType type = ServerAdminCommandType::Unknown;
	};

	[[nodiscard]] ServerAdminCommand ParseServerAdminCommand(std::string_view commandLine) noexcept;
}