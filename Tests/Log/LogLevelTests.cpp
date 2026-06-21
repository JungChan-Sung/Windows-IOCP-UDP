#include "LogLevelTests.h"

#include <Common/Log/LogLevel.h>

#include <Tests/TestHelpers.h>

namespace tests::log
{
	tests::DebugTestResult RunLogLevelTests()
	{
		tests::DebugTestResult result{};

		tests::Expect(result, common::log::ToString(common::log::LogLevel::Trace) == "Trace", "LogLevel: trace string");
		tests::Expect(result, common::log::ToString(common::log::LogLevel::Debug) == "Debug", "LogLevel: debug string");
		tests::Expect(result, common::log::ToString(common::log::LogLevel::Info) == "Info", "LogLevel: info string");
		tests::Expect(result, common::log::ToString(common::log::LogLevel::Warning) == "Warning", "LogLevel: warning string");
		tests::Expect(result, common::log::ToString(common::log::LogLevel::Error) == "Error", "LogLevel: error string");

		return result;
	}
}