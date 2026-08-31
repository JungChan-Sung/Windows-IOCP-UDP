#include "ServerAdminCommand.h"

#include <cstddef>
#include <string_view>

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
}

namespace server::admin
{
	ServerAdminCommand ParseServerAdminCommand(std::string_view commandLine) noexcept
	{
		commandLine = Trim(commandLine);

		if (EqualsIgnoreCase(commandLine, "help"))
		{
			return ServerAdminCommand{
				.type = ServerAdminCommandType::Help,
			};
		}

		if (EqualsIgnoreCase(commandLine, "status"))
		{
			return ServerAdminCommand{
				.type = ServerAdminCommandType::Status,
			};
		}

		if (EqualsIgnoreCase(commandLine, "players"))
		{
			return ServerAdminCommand{
				.type = ServerAdminCommandType::Players,
			};
		}

		if (EqualsIgnoreCase(commandLine, "rooms"))
		{
			return ServerAdminCommand{
				.type = ServerAdminCommandType::Rooms,
			};
		}

		if (EqualsIgnoreCase(commandLine, "stop"))
		{
			return ServerAdminCommand{
				.type = ServerAdminCommandType::Stop,
			};
		}

		return {};
	}
}