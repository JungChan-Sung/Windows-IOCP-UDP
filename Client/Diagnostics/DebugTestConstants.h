#pragma once

#include <cstdint>
#include <chrono>

namespace client::diagnostics
{
	inline constexpr std::int32_t roomIdA = 1;
	inline constexpr std::int32_t roomIdB = 2;

	inline constexpr std::uint32_t serverTickA = 100;
	inline constexpr std::uint32_t serverTickB = 200;
	inline constexpr std::uint32_t serverTickC = 300;
	inline constexpr std::uint32_t serverTickD = 400;
	inline constexpr std::uint32_t serverTickE = 500;

	inline constexpr std::uint32_t bulletIdBaseA = 1000;
	inline constexpr std::uint32_t bulletIdBaseB = 2000;
	inline constexpr std::uint32_t bulletIdBaseC = 3000;
	inline constexpr std::uint32_t bulletIdBaseD = 4000;
	inline constexpr std::uint32_t bulletIdBaseE = 5000;

	inline constexpr float testX0 = 10.0F;
	inline constexpr float testY0 = 20.0F;
	inline constexpr float testX1 = 30.0F;
	inline constexpr float testY1 = 40.0F;
	inline constexpr float testX2 = 50.0F;
	inline constexpr float testY2 = 60.0F;
	inline constexpr float testX3 = 70.0F;
	inline constexpr float testY3 = 80.0F;
	inline constexpr float testX4 = 90.0F;
	inline constexpr float testY4 = 100.0F;
	inline constexpr float testX5 = 110.0F;
	inline constexpr float testY5 = 120.0F;

	inline constexpr std::chrono::milliseconds chunkAssemblyTimeoutWait{ 300 };
}