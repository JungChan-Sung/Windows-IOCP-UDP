#pragma once

#include <cstdint>
#include <string_view>

#include <Common/Packet/PacketHeader.h>

namespace common::packet
{
	enum class ServerDisconnectReason : std::uint8_t
	{
		None,
		Kicked,

		Count,
	};

	struct KeepAlivePacket
	{
	public:
		PacketHeader header{ 0, PacketType::KeepAlive };
	};

	struct ServerDisconnectPacket
	{
	public:
		PacketHeader header{ 0, PacketType::ServerDisconnect };
		ServerDisconnectReason reason = ServerDisconnectReason::None;
	};

	[[nodiscard]] inline constexpr std::string_view ToString(ServerDisconnectReason reason) noexcept
	{
		switch (reason)
		{
		case ServerDisconnectReason::None:
			return "None";

		case ServerDisconnectReason::Kicked:
			return "Kicked";

		default:
			return "Unknown";
		}
	}
}