#include "PeerRoomManager.h"

namespace server::net
{
	void PeerRoomManager::Clear() noexcept
	{
		peerTable_.clear();
		roomTable_.clear();
	}

	PeerState* PeerRoomManager::FindPeer(const EndpointKey& endpointKey) noexcept
	{
		const auto peerIterator = peerTable_.find(endpointKey);
		if (peerIterator == peerTable_.end())
		{
			return nullptr;
		}

		return &peerIterator->second;
	}

	const PeerState* PeerRoomManager::FindPeer(const EndpointKey& endpointKey) const noexcept
	{
		const auto peerIterator = peerTable_.find(endpointKey);
		if (peerIterator == peerTable_.end())
		{
			return nullptr;
		}

		return &peerIterator->second;
	}

	PeerState* PeerRoomManager::FindJoinedPeer(const EndpointKey& endpointKey) noexcept
	{
		PeerState* peerState = FindPeer(endpointKey);
		if (peerState == nullptr)
		{
			return nullptr;
		}

		if (!peerState->isJoined)
		{
			return nullptr;
		}

		return peerState;
	}

	const PeerState* PeerRoomManager::FindJoinedPeer(const EndpointKey& endpointKey) const noexcept
	{
		const PeerState* peerState = FindPeer(endpointKey);
		if (peerState == nullptr)
		{
			return nullptr;
		}

		if (!peerState->isJoined)
		{
			return nullptr;
		}

		return peerState;
	}

	PeerState* PeerRoomManager::FindJoinedPeerByAccountId(std::int64_t accountId) noexcept
	{
		if (accountId <= 0)
		{
			return nullptr;
		}

		for (auto& [_, peerState] : peerTable_)
		{
			if (!peerState.isJoined)
			{
				continue;
			}

			if (peerState.accountId == accountId)
			{
				return &peerState;
			}
		}

		return nullptr;
	}

	const PeerState* PeerRoomManager::FindJoinedPeerByAccountId(std::int64_t accountId) const noexcept
	{
		if (accountId <= 0)
		{
			return nullptr;
		}

		for (const auto& [_, peerState] : peerTable_)
		{
			if (!peerState.isJoined)
			{
				continue;
			}

			if (peerState.accountId == accountId)
			{
				return &peerState;
			}
		}

		return nullptr;
	}

	const PeerRoomManager::RoomMemberSet* PeerRoomManager::FindRoomMemberSet(RoomId roomId) const noexcept
	{
		const auto roomIterator = roomTable_.find(roomId);
		if (roomIterator == roomTable_.end())
		{
			return nullptr;
		}

		return &roomIterator->second;
	}

	PeerState& PeerRoomManager::UpsertJoinedPeer(const sockaddr_in& remoteAddress, const EndpointKey& endpointKey, PlayerId playerId, RoomId roomId, TimePoint currentTime)
	{
		PeerState& peerState = peerTable_[endpointKey];
		peerState.remoteAddress = remoteAddress;
		peerState.endpointKey = endpointKey;
		peerState.playerId = playerId;
		peerState.roomId = roomId;
		peerState.isJoined = true;
		peerState.lastRecvTime = currentTime;

		roomTable_[roomId].insert(endpointKey);
		return peerState;
	}

	void PeerRoomManager::ForEachJoinedPeer(const std::function<void(PeerState&)>& action)
	{
		for (auto& [_, peerState] : peerTable_)
		{
			if (!peerState.isJoined)
			{
				continue;
			}

			action(peerState);
		}
	}

	void PeerRoomManager::ForEachJoinedPeer(const std::function<void(const PeerState&)>& action) const
	{
		for (const auto& [_, peerState] : peerTable_)
		{
			if (!peerState.isJoined)
			{
				continue;
			}

			action(peerState);
		}
	}

	void PeerRoomManager::RefreshRecvTime(const EndpointKey& endpointKey, TimePoint currentTime) noexcept
	{
		PeerState* peerState = FindPeer(endpointKey);
		if (peerState == nullptr)
		{
			return;
		}

		peerState->lastRecvTime = currentTime;
	}

	bool PeerRoomManager::RemovePeer(const EndpointKey& endpointKey, PlayerId& playerId, RoomId& roomId) noexcept
	{
		const auto peerIterator = peerTable_.find(endpointKey);
		if (peerIterator == peerTable_.end())
		{
			return false;
		}

		const PeerState& peerState = peerIterator->second;
		if (!peerState.isJoined)
		{
			peerTable_.erase(peerIterator);
			return false;
		}

		playerId = peerState.playerId;
		roomId = peerState.roomId;

		auto roomIterator = roomTable_.find(roomId);
		if (roomIterator != roomTable_.end())
		{
			roomIterator->second.erase(endpointKey);

			if (roomIterator->second.empty())
			{
				roomTable_.erase(roomIterator);
			}
		}

		peerTable_.erase(peerIterator);
		return true;
	}

	bool PeerRoomManager::ChangePeerRoom(const EndpointKey& endpointKey, RoomId nextRoomId, TimePoint currentTime, RoomChangeResult& roomChangeResult) noexcept
	{
		PeerState* peerState = FindJoinedPeer(endpointKey);
		if (peerState == nullptr)
		{
			return false;
		}

		if (peerState->roomId == nextRoomId)
		{
			return false;
		}

		roomChangeResult.playerId = peerState->playerId;
		roomChangeResult.previousRoomId = peerState->roomId;
		roomChangeResult.nextRoomId = nextRoomId;
		roomChangeResult.remoteAddress = peerState->remoteAddress;

		auto previousRoomIterator = roomTable_.find(peerState->roomId);
		if (previousRoomIterator != roomTable_.end())
		{
			previousRoomIterator->second.erase(endpointKey);

			if (previousRoomIterator->second.empty())
			{
				roomTable_.erase(previousRoomIterator);
			}
		}

		roomTable_[nextRoomId].insert(endpointKey);

		peerState->roomId = nextRoomId;
		peerState->lastRecvTime = currentTime;
		return true;
	}

	std::vector<PeerRoomManager::TimedOutPeer> PeerRoomManager::RemoveTimedOutPeers(TimePoint currentTime, Duration timeout) noexcept
	{
		std::vector<TimedOutPeer> timedOutPeerList;

		for (auto peerIterator = peerTable_.begin(); peerIterator != peerTable_.end();)
		{
			if (currentTime - peerIterator->second.lastRecvTime <= timeout)
			{
				++peerIterator;
				continue;
			}

			if (peerIterator->second.isJoined)
			{
				TimedOutPeer timedOutPeer{};
				timedOutPeer.endpointKey = peerIterator->first;
				timedOutPeer.playerId = peerIterator->second.playerId;
				timedOutPeer.roomId = peerIterator->second.roomId;
				timedOutPeerList.push_back(timedOutPeer);

				auto roomIterator = roomTable_.find(peerIterator->second.roomId);
				if (roomIterator != roomTable_.end())
				{
					roomIterator->second.erase(peerIterator->first);

					if (roomIterator->second.empty())
					{
						roomTable_.erase(roomIterator);
					}
				}
			}

			peerIterator = peerTable_.erase(peerIterator);
		}

		return timedOutPeerList;
	}

	PeerRoomManager::RemoteAddressList PeerRoomManager::BuildRoomRemoteAddressList(RoomId roomId) const
	{
		RemoteAddressList remoteAddressList;

		const RoomMemberSet* roomMemberSet = FindRoomMemberSet(roomId);
		if (roomMemberSet == nullptr)
		{
			return remoteAddressList;
		}

		remoteAddressList.reserve(roomMemberSet->size());

		for (const EndpointKey& endpointKey : *roomMemberSet)
		{
			const PeerState* peerState = FindPeer(endpointKey);
			if (peerState == nullptr || !peerState->isJoined)
			{
				continue;
			}

			remoteAddressList.push_back(peerState->remoteAddress);
		}

		return remoteAddressList;
	}

	std::size_t PeerRoomManager::GetJoinedPeerCount() const noexcept
	{
		std::size_t joinedPeerCount = 0;

		for (const auto& [_, peerState] : peerTable_)
		{
			if (peerState.isJoined)
			{
				++joinedPeerCount;
			}
		}

		return joinedPeerCount;
	}
}