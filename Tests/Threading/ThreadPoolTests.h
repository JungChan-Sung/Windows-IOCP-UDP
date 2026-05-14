#pragma once

#include <Common/Diagnostics/DebugTestResult.h>

namespace tests::threading
{
	[[nodiscard]] common::diagnostics::DebugTestResult RunThreadPoolTests();
}