#pragma once

#include <WinSock2.h>

#include <vector>

#include <Common/Packet/Game/GamePacket.h>

namespace server::net
{
	using RemoteAddressList = std::vector<sockaddr_in>;

	struct PlayerSnapshotTask
	{
	public:
		sockaddr_in remoteAddress{};
		common::packet::PlayerSnapshotPacket snapshotPacket{};
	};

	struct BulletSnapshotTask
	{
	public:
		RemoteAddressList remoteAddressList;
		common::packet::BulletSnapshotPacket snapshotPacket{};
	};

	struct ImpactEffectTask
	{
	public:
		RemoteAddressList remoteAddressList;
		common::packet::ImpactEffectPacket effectPacket{};
	};
}