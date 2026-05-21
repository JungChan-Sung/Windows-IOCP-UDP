#pragma once

#include <string_view>

#include <Tests/DebugTestResult.h>

namespace tests
{
	class TestRunner
	{
	public:
		TestRunner() = delete;
		~TestRunner() = delete;

		TestRunner(const TestRunner&) = delete;
		TestRunner& operator=(const TestRunner&) = delete;

		TestRunner(TestRunner&&) = delete;
		TestRunner& operator=(TestRunner&&) = delete;

	public:
		[[nodiscard]] static bool RunAll();

	private:
		static void PrintResult(std::string_view testName, const tests::DebugTestResult& result);
		static void MergeAndPrint(
			tests::DebugTestResult& totalResult,
			std::string_view testName,
			const tests::DebugTestResult& result
		);
	};
}