#pragma once

#include <Common/Net/Endpoint.h>
#include <Common/Packet/Game/GamePacket.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameWorld.h>
#include <Server/Net/PeerRoomManager.h>
#include <Server/Net/PeerState.h>

namespace server::service
{
	class PlayerCommandService
	{
	private:
		struct PeerPlayerView
		{
		public:
			net::PeerState* peerState = nullptr;
			game::PlayerState* playerState = nullptr;
		};

	public:
		using EndpointKey = common::net::EndpointKey;
		using TimePoint = common::time::TimePoint;

	public:
		PlayerCommandService() = default;
		~PlayerCommandService() noexcept = default;

		PlayerCommandService(const PlayerCommandService&) = delete;
		PlayerCommandService& operator=(const PlayerCommandService&) = delete;

		PlayerCommandService(PlayerCommandService&&) = delete;
		PlayerCommandService& operator=(PlayerCommandService&&) = delete;

	public:
		[[nodiscard]] bool ApplyInputCommand(
			const EndpointKey& endpointKey,
			const common::packet::InputCommandPacket& packet,
			net::PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			TimePoint currentTime
		) const;

		[[nodiscard]] bool FireBullet(const EndpointKey& endpointKey,
			net::PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			const game::GameSimulation& gameSimulation,
			const config::WeaponRuleConfig& weaponRuleConfig, 
			TimePoint currentTime
		) const;

	private:
		[[nodiscard]] PeerPlayerView FindJoinedPeerPlayer(
			const EndpointKey& endpointKey,
			net::PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld
		) const;
	};
}

