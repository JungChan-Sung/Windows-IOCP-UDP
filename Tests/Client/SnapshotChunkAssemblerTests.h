#pragma once

#include <Common/Diagnostics/DebugTestResult.h>

namespace tests::client
{
	[[nodiscard]] common::diagnostics::DebugTestResult RunSnapshotChunkAssemblerTests();
}