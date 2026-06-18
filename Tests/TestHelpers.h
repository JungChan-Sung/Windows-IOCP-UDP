#pragma once

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <thread>

#include <Common/Time/TimeTypes.h>

namespace tests
{
	[[nodiscard]] inline std::filesystem::path MakeTempFilePath(std::string_view fileName)
	{
		return std::filesystem::temp_directory_path() / std::filesystem::path(fileName);
	}

	inline void WriteTextFile(const std::filesystem::path& filePath, std::string_view text)
	{
		std::ofstream file(filePath, std::ios::trunc);
		file << text;
	}

	template <typename TPredicate>
	[[nodiscard]] bool WaitUntil(TPredicate predicate, common::time::Milliseconds timeout)
	{
		const auto startTime = std::chrono::steady_clock::now();

		while (!predicate())
		{
			if (std::chrono::steady_clock::now() - startTime >= timeout)
			{
				return false;
			}

			std::this_thread::sleep_for(common::time::Milliseconds(1));
		}

		return true;
	}

	template <typename TWarningList>
	[[nodiscard]] bool ContainsWarningMessage(const TWarningList& warningList, std::string_view message)
	{
		return std::ranges::any_of(
			warningList,
			[message](const auto& warning)
			{
				return warning.message == message;
			}
		);
	}
}