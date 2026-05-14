#pragma once

#include <cstddef>
#include <string>

namespace server::config
{
	struct ServerConfigWarning
	{
	public:
		std::size_t lineNumber = 0;
		std::string message;
	};
}