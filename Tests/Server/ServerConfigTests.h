#pragma once

#include <Common/Diagnostics/DebugTestResult.h>

namespace tests::server
{
	[[nodiscard]] common::diagnostics::DebugTestResult RunServerConfigTests();
}