#include "ServerAdminCommandTests.h"

#include <Server/Admin/ServerAdminCommand.h>

namespace
{
	void RunEmptyCommandTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Unknown,
			"ServerAdminCommand: empty command is unknown"
		);
	}

	void RunHelpCommandTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("help");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Help,
			"ServerAdminCommand: parses help"
		);
	}

	void RunStatusCommandTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("status");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Status,
			"ServerAdminCommand: parses status"
		);
	}

	void RunPlayersCommandTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("players");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Players,
			"ServerAdminCommand: parses players"
		);
	}

	void RunRoomsCommandTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("rooms");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Rooms,
			"ServerAdminCommand: parses rooms"
		);
	}

	void RunStopCommandTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("stop");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Stop,
			"ServerAdminCommand: parses stop"
		);
	}

	void RunWhitespaceTrimTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand(" \t status \t ");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Status,
			"ServerAdminCommand: trims whitespace"
		);
	}

	void RunCaseInsensitiveTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("PlAyErS");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Players,
			"ServerAdminCommand: command names are case insensitive"
		);
	}

	void RunUnknownCommandTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("unknown");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Unknown,
			"ServerAdminCommand: unknown command rejected"
		);
	}

	void RunUnexpectedArgumentTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("status extra");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Unknown,
			"ServerAdminCommand: unexpected arguments rejected"
		);
	}
}

namespace tests::server
{
	DebugTestResult RunServerAdminCommandTests()
	{
		DebugTestResult result{};

		RunEmptyCommandTest(result);
		RunHelpCommandTest(result);
		RunStatusCommandTest(result);
		RunPlayersCommandTest(result);
		RunRoomsCommandTest(result);
		RunStopCommandTest(result);
		RunWhitespaceTrimTest(result);
		RunCaseInsensitiveTest(result);
		RunUnknownCommandTest(result);
		RunUnexpectedArgumentTest(result);

		return result;
	}
}