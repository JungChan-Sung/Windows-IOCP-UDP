#pragma once

#include <string>
#include <string_view>

namespace common::string
{
	[[nodiscard]] inline std::string FormatScopedName(std::string_view scope, std::string_view name)
	{
		std::string result;
		result.reserve(scope.size() + 1 + name.size());
		result.append(scope);
		result.push_back('.');
		result.append(name);
		return result;
	}
}