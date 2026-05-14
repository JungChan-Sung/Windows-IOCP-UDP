#pragma once

#include <Common/Diagnostics/DebugTestResult.h>

namespace tests::packet
{
	[[nodiscard]] common::diagnostics::DebugTestResult RunPacketSerializationTests();
}