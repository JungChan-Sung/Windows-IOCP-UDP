#pragma once

#include <string_view>

namespace client::config
{
	enum class ClientTransportType
	{
		Socket,
		Iocp
	};

	[[nodiscard]] constexpr std::string_view ToString(ClientTransportType transportType) noexcept
	{
		switch (transportType)
		{
		case ClientTransportType::Socket:
			return "Socket";

		case ClientTransportType::Iocp:
			return "Iocp";

		default:
			return "Unknown";
		}
	}
}