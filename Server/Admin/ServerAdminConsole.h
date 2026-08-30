#pragma once

#include <cstddef>
#include <string>

namespace server::admin
{
	class ServerAdminConsole
	{
	public:
		enum class EventType
		{
			None,
			CommandLine,
			StopRequested,
		};

		struct Event
		{
			EventType type = EventType::None;
			std::string commandLine;
		};

	private:
		static inline constexpr std::size_t maxCommandLineLength = 256;

	private:
		std::string commandLine_;

	public:
		ServerAdminConsole() = default;
		~ServerAdminConsole() noexcept = default;

		ServerAdminConsole(const ServerAdminConsole&) = delete;
		ServerAdminConsole& operator=(const ServerAdminConsole&) = delete;

		ServerAdminConsole(ServerAdminConsole&&) = delete;
		ServerAdminConsole& operator=(ServerAdminConsole&&) = delete;

	public:
		void Clear() noexcept;

		[[nodiscard]] Event Poll();
	};
}