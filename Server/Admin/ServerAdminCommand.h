#pragma once

#include <string_view>

#include <Common/Game/GameTypes.h>

namespace server::admin
{
	enum class ServerAdminCommandType
	{
		Unknown,
		Help,
		Status,
		Players,
		Rooms,
		Kick,
		Stop,
	};

	struct ServerAdminCommand
	{
		ServerAdminCommandType type = ServerAdminCommandType::Unknown;
		common::game::PlayerId playerId = 0;
	};

	[[nodiscard]] ServerAdminCommand ParseServerAdminCommand(std::string_view commandLine) noexcept;
}