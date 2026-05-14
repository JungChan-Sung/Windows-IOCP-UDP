#pragma once

#include <Common/Diagnostics/DebugTestResult.h>

namespace tests::log
{
	[[nodiscard]] common::diagnostics::DebugTestResult RunAsyncLogWriterTests();
}