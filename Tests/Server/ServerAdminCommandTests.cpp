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

	void RunKickCommandTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("kick 42");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Kick,
			"ServerAdminCommand: parses kick"
		);

		tests::Expect(
			result,
			command.playerId == 42,
			"ServerAdminCommand: parses kick player id"
		);
	}

	void RunKickCaseInsensitiveTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("KiCk\t12");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Kick
			&& command.playerId == 12,
			"ServerAdminCommand: kick is case insensitive"
		);
	}

	void RunKickMissingPlayerIdTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("kick");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Unknown,
			"ServerAdminCommand: kick requires player id"
		);
	}

	void RunKickZeroPlayerIdTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("kick 0");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Unknown,
			"ServerAdminCommand: kick rejects zero player id"
		);
	}

	void RunKickInvalidPlayerIdTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("kick player");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Unknown,
			"ServerAdminCommand: kick rejects invalid player id"
		);
	}

	void RunKickExtraArgumentTest(tests::DebugTestResult& result)
	{
		const server::admin::ServerAdminCommand command =
			server::admin::ParseServerAdminCommand("kick 12 extra");

		tests::Expect(
			result,
			command.type == server::admin::ServerAdminCommandType::Unknown,
			"ServerAdminCommand: kick rejects extra argument"
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
		RunKickCommandTest(result);
		RunKickCaseInsensitiveTest(result);
		RunKickMissingPlayerIdTest(result);
		RunKickZeroPlayerIdTest(result);
		RunKickInvalidPlayerIdTest(result);
		RunKickExtraArgumentTest(result);
		RunStopCommandTest(result);
		RunWhitespaceTrimTest(result);
		RunCaseInsensitiveTest(result);
		RunUnknownCommandTest(result);
		RunUnexpectedArgumentTest(result);

		return result;
	}
}