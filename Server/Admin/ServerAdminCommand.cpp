#include "ServerAdminCommand.h"

#include <charconv>
#include <cstddef>
#include <string_view>
#include <system_error>

namespace
{
	[[nodiscard]] constexpr char ToLowerAscii(char value) noexcept
	{
		if (value >= 'A' && value <= 'Z')
		{
			return static_cast<char>(value + ('a' - 'A'));
		}

		return value;
	}

	[[nodiscard]] bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs) noexcept
	{
		if (lhs.size() != rhs.size())
		{
			return false;
		}

		for (std::size_t index = 0; index < lhs.size(); ++index)
		{
			if (ToLowerAscii(lhs[index]) != ToLowerAscii(rhs[index]))
			{
				return false;
			}
		}

		return true;
	}

	[[nodiscard]] std::string_view Trim(std::string_view value) noexcept
	{
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
		{
			value.remove_prefix(1);
		}

		while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
		{
			value.remove_suffix(1);
		}

		return value;
	}

	[[nodiscard]] bool ParsePlayerId(std::string_view value, common::game::PlayerId& playerId) noexcept
	{
		if (value.empty())
		{
			return false;
		}

		const char* begin = value.data();
		const char* end = begin + value.size();

		const auto [parseEnd, error] = std::from_chars(begin, end, playerId);
		return error == std::errc{} && parseEnd == end && playerId != 0;
	}
}

namespace server::admin
{
	ServerAdminCommand ParseServerAdminCommand(std::string_view commandLine) noexcept
	{
		commandLine = Trim(commandLine);

		const std::size_t separatorIndex = commandLine.find_first_of(" \t");

		const std::string_view commandName =
			(separatorIndex == std::string_view::npos)
			? commandLine
			: commandLine.substr(0, separatorIndex);

		const std::string_view argument =
			(separatorIndex == std::string_view::npos)
			? std::string_view{}
		: Trim(commandLine.substr(separatorIndex + 1));

		if (EqualsIgnoreCase(commandName, "help") && argument.empty())
		{
			return ServerAdminCommand{
				.type = ServerAdminCommandType::Help,
			};
		}

		if (EqualsIgnoreCase(commandName, "status") && argument.empty())
		{
			return ServerAdminCommand{
				.type = ServerAdminCommandType::Status,
			};
		}

		if (EqualsIgnoreCase(commandName, "players") && argument.empty())
		{
			return ServerAdminCommand{
				.type = ServerAdminCommandType::Players,
			};
		}

		if (EqualsIgnoreCase(commandName, "rooms") && argument.empty())
		{
			return ServerAdminCommand{
				.type = ServerAdminCommandType::Rooms,
			};
		}

		if (EqualsIgnoreCase(commandName, "kick"))
		{
			common::game::PlayerId playerId = 0;
			if (!ParsePlayerId(argument, playerId))
			{
				return {};
			}

			return ServerAdminCommand{
				.type = ServerAdminCommandType::Kick,
				.playerId = playerId,
			};
		}

		if (EqualsIgnoreCase(commandName, "stop") && argument.empty())
		{
			return ServerAdminCommand{
				.type = ServerAdminCommandType::Stop,
			};
		}

		return {};
	}
}