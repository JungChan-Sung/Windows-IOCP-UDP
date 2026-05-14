#pragma once

#include <Common/Diagnostics/DebugTestResult.h>

namespace tests::net
{
	[[nodiscard]] common::diagnostics::DebugTestResult RunSnapshotChunkAssemblerCoreTests();
}