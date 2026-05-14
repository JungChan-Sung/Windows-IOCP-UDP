#pragma once

#include <cstddef>
#include <string>

namespace client::config
{
	struct ClientConfigWarning
	{
	public:
		std::size_t lineNumber = 0;
		std::string message;
	};
}