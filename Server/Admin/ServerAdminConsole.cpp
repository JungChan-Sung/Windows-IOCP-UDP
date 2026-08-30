#include "ServerAdminConsole.h"

#include <conio.h>

#include <iostream>
#include <utility>

namespace
{
	inline constexpr int extendedKeyPrefix = 0;
	inline constexpr int extendedKeyPrefix2 = 0xE0;

	inline constexpr int backspaceKey = '\b';
	inline constexpr int enterKey = '\r';
	inline constexpr int escapeKey = 27;

	[[nodiscard]] bool IsPrintableAscii(int key) noexcept
	{
		return key >= ' ' && key <= '~';
	}

	void EchoCharacter(char character)
	{
		std::cout.put(character);
		std::cout.flush();
	}

	void EchoBackspace()
	{
		std::cout << "\b \b";
		std::cout.flush();
	}

	void EchoNewLine()
	{
		std::cout.put('\n');
		std::cout.flush();
	}
}

namespace server::admin
{
	void ServerAdminConsole::Clear() noexcept
	{
		commandLine_.clear();
	}

	ServerAdminConsole::Event ServerAdminConsole::Poll()
	{
		while (::_kbhit() != 0)
		{
			const int key = ::_getch();

			if (key == extendedKeyPrefix || key == extendedKeyPrefix2)
			{
				static_cast<void>(::_getch());
				continue;
			}

			if (key == escapeKey)
			{
				Clear();
				EchoNewLine();

				return Event{
					.type = EventType::StopRequested,
				};
			}

			if (key == enterKey)
			{
				EchoNewLine();

				if (commandLine_.empty())
				{
					return {};
				}

				Event event{};
				event.type = EventType::CommandLine;
				event.commandLine = std::move(commandLine_);

				commandLine_.clear();

				return event;
			}

			if (key == backspaceKey)
			{
				if (!commandLine_.empty())
				{
					commandLine_.pop_back();
					EchoBackspace();
				}

				continue;
			}

			if (!IsPrintableAscii(key) || commandLine_.size() >= maxCommandLineLength)
			{
				continue;
			}

			const char character = static_cast<char>(key);

			commandLine_.push_back(character);
			EchoCharacter(character);
		}

		return {};
	}
}