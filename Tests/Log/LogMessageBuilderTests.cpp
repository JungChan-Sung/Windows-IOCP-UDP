#include "LogMessageBuilderTests.h"

#include <string>

#include <Common/Log/LogMessageBuilder.h>

#include <Tests/TestHelpers.h>

namespace tests::log
{
	tests::DebugTestResult RunLogMessageBuilderTests()
	{
		tests::DebugTestResult result{};

		const std::string message =
			common::log::LogMessageBuilder{}
			.Append("Port=")
			.Append(9000)
			.Append(", Enabled=")
			.Append(true)
			.Build();

		tests::Expect(
			result,
			message == "Port=9000, Enabled=true",
			"LogMessageBuilder: append values"
		);

		const std::string namedValueMessage =
			common::log::LogMessageBuilder{}
			.AppendNamedValue("Port", 9000)
			.AppendCommaNamedValue("Enabled", true)
			.AppendCommaNamedValue("Name", "Server")
			.Build();

		tests::Expect(
			result,
			namedValueMessage == "Port=9000, Enabled=true, Name=Server",
			"LogMessageBuilder: append named values"
		);

		const std::string emptyMessage =
			common::log::LogMessageBuilder{}
		.Build();

		tests::Expect(
			result,
			emptyMessage.empty(),
			"LogMessageBuilder: empty message"
		);

		return result;
	}
}