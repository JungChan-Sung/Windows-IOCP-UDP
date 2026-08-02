#pragma once

#include <WinSock2.h>

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Common/Game/GameTypes.h>
#include <Common/Net/Endpoint.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Net/PeerState.h>

namespace server::net
{
	class PeerRoomManager
	{
	public:
		using PlayerId = common::game::PlayerId;
		using RoomId = common::game::RoomId;
		using EndpointKey = common::net::EndpointKey;

		using Clock = common::time::Clock;
		using TimePoint = common::time::TimePoint;
		using Duration = common::time::Duration;

	public:
		struct TimedOutPeer
		{
		public:
			EndpointKey endpointKey{};
			PlayerId playerId = 0;
			RoomId roomId = 0;
		};

		struct RoomChangeResult
		{
		public:
			PlayerId playerId = 0;
			RoomId previousRoomId = 0;
			RoomId nextRoomId = 0;
			sockaddr_in remoteAddress{};
		};

	public:
		using PeerTable = std::unordered_map<EndpointKey, PeerState, common::net::EndpointKeyHasher>;
		using RoomMemberSet = std::unordered_set<EndpointKey, common::net::EndpointKeyHasher>;
		using RoomTable = std::unordered_map<RoomId, RoomMemberSet>;
		using RemoteAddressList = std::vector<sockaddr_in>;
		using TimedOutPeerList = std::vector<TimedOutPeer>;

	private:
		PeerTable peerTable_;
		RoomTable roomTable_;

	public:
		PeerRoomManager() = default;
		~PeerRoomManager() noexcept = default;

		PeerRoomManager(const PeerRoomManager&) = delete;
		PeerRoomManager& operator=(const PeerRoomManager&) = delete;

		PeerRoomManager(PeerRoomManager&&) = delete;
		PeerRoomManager& operator=(PeerRoomManager&&) = delete;

	public:
		void Clear() noexcept;

		[[nodiscard]] PeerState* FindPeer(const EndpointKey& endpointKey) noexcept;
		[[nodiscard]] const PeerState* FindPeer(const EndpointKey& endpointKey) const noexcept;
		[[nodiscard]] PeerState* FindJoinedPeer(const EndpointKey& endpointKey) noexcept;
		[[nodiscard]] PeerState* FindJoinedPeerByAccountId(std::int64_t accountId) noexcept;
		[[nodiscard]] const PeerState* FindJoinedPeerByAccountId(std::int64_t accountId) const noexcept;
		[[nodiscard]] const RoomMemberSet* FindRoomMemberSet(RoomId roomId) const noexcept;

		[[nodiscard]] PeerState& UpsertJoinedPeer(
			const sockaddr_in& remoteAddress,
			const EndpointKey& endpointKey,
			PlayerId playerId,
			RoomId roomId,
			TimePoint currentTime
		);

		void ForEachJoinedPeer(const std::function<void(PeerState&)>& action);
		void ForEachJoinedPeer(const std::function<void(const PeerState&)>& action) const;

		void RefreshRecvTime(const EndpointKey& endpointKey, TimePoint currentTime) noexcept;

		[[nodiscard]] bool RemovePeer(const EndpointKey& endpointKey, PlayerId& playerId, RoomId& roomId) noexcept;

		[[nodiscard]] bool ChangePeerRoom(
			const EndpointKey& endpointKey, 
			RoomId nextRoomId, 
			TimePoint currentTime,
			RoomChangeResult& roomChangeResult
		) noexcept;

		[[nodiscard]] TimedOutPeerList RemoveTimedOutPeers(TimePoint currentTime, Duration timeout) noexcept;

		[[nodiscard]] RemoteAddressList BuildRoomRemoteAddressList(RoomId roomId) const;

	public:
		[[nodiscard]] const PeerTable& GetPeerTable() const noexcept
		{
			return peerTable_;
		}

		[[nodiscard]] const RoomTable& GetRoomTable() const noexcept
		{
			return roomTable_;
		}

		[[nodiscard]] std::size_t GetRoomMemberCount(RoomId roomId) const noexcept
		{
			const RoomMemberSet* roomMemberSet = FindRoomMemberSet(roomId);
			return (roomMemberSet != nullptr) ? roomMemberSet->size() : 0;
		}

		[[nodiscard]] std::size_t GetPeerCount() const noexcept
		{
			return peerTable_.size();
		}

		[[nodiscard]] std::size_t GetRoomCount() const noexcept
		{
			return roomTable_.size();
		}

		[[nodiscard]] std::size_t GetJoinedPeerCount() const noexcept;
	};
}