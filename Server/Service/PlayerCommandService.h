#pragma once

#include <cstdint>

#include <Common/Game/InputFlags.h>
#include <Common/Game/WeaponRules.h>
#include <Common/Net/EndpointKey.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Game/GameWorld.h>
#include <Server/Service/PeerRoomManager.h>
#include <Server/Service/PeerState.h>

namespace server::service
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
			std::uint32_t inputSequence,
			common::game::InputFlags inputFlags,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			TimePoint currentTime
		) const;

		[[nodiscard]] bool FireBullet(
			const EndpointKey& endpointKey,
			PeerRoomManager& peerRoomManager,
			game::GameWorld& gameWorld,
			const common::game::WeaponRuleConfig& weaponRuleConfig, 
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

