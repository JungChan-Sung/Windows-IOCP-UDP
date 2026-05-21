#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tests
{
	struct DebugTestResult
	{
	public:
		int passedCount = 0;
		int failedCount = 0;
		std::vector<std::string> failureList;

	public:
		void AddPassed() noexcept
		{
			++passedCount;
		}

		void AddFailed(std::string_view message)
		{
			++failedCount;
			failureList.emplace_back(message);
		}

		void Merge(const DebugTestResult& other)
		{
			passedCount += other.passedCount;
			failedCount += other.failedCount;
			failureList.insert(failureList.end(), other.failureList.begin(), other.failureList.end());
		}

	public:
		[[nodiscard]] bool IsSucceeded() const noexcept
		{
			return failedCount == 0;
		}
	};

	inline void Expect(DebugTestResult& result, bool condition, std::string_view message)
	{
		if (condition)
		{
			result.AddPassed();
			return;
		}

		result.AddFailed(message);
	}
}