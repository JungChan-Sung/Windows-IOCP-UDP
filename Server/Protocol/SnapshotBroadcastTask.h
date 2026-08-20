#pragma once

#include <vector>

#include <Common/Net/EndpointKey.h>
#include <Common/Packet/Game/GamePacket.h>

namespace server::protocol
{
	using EndpointKeyList = std::vector<common::net::EndpointKey>;

	struct PlayerSnapshotTask
	{
	public:
		common::net::EndpointKey endpointKey{};
		common::packet::PlayerSnapshotPacket snapshotPacket{};
	};

	struct BulletSnapshotTask
	{
	public:
		EndpointKeyList endpointKeyList;
		common::packet::BulletSnapshotPacket snapshotPacket{};
	};

	struct ImpactEffectTask
	{
	public:
		EndpointKeyList endpointKeyList;
		common::packet::ImpactEffectPacket effectPacket{};
	};
}