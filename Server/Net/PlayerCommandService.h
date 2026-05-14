#pragma once

#include <chrono>

#include <Common/Net/Endpoint.h>
#include <Common/Packet/GamePacket.h>

#include <Server/Config/ServerConfig.h>
#include <Server/Game/GameSimulation.h>
#include <Server/Game/GameWorld.h>
#include <Server/Net/PeerRoomManager.h>
#include <Server/Net/PeerState.h>

namespace server::net
{
	class PlayerCommandService
	{
	private:
		struct PeerPlayerView
		{
		public:
			PeerState* peerState = nullptr;
			game::PlayerState* playerState = nullptr;
		};

	public:
		using EndpointKey = common::net::EndpointKey;
		using TimePoint = std::chrono::steady_clock::time_point;

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
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			TimePoint currentTime
		) const;

		[[nodiscard]] bool FireBullet(const EndpointKey& endpointKey,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			const game::GameSimulation& gameSimulation,
			const config::WeaponRuleConfig& weaponRuleConfig, 
			TimePoint currentTime
		) const;

	private:
		[[nodiscard]] PeerPlayerView FindJoinedPeerPlayer(
			const EndpointKey& endpointKey,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld
		) const;
	};
}

