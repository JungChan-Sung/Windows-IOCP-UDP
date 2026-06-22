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
			message == "Port=9000, Enabled=1",
			"LogMessageBuilder: append values"
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