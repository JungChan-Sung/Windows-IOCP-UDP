#include "PeerRoomManager.h"

namespace server::service
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

	PeerState* PeerRoomManager::FindJoinedPeerByAccountId(common::identity::AccountId accountId) noexcept
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

	const PeerState* PeerRoomManager::FindJoinedPeerByAccountId(common::identity::AccountId accountId) const noexcept
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

	PeerState* PeerRoomManager::FindJoinedPeerByPlayerId(PlayerId playerId) noexcept
	{
		for (auto& [_, peerState] : peerTable_)
		{
			if (peerState.isJoined && peerState.playerId == playerId)
			{
				return &peerState;
			}
		}

		return nullptr;
	}

	const PeerState* PeerRoomManager::FindJoinedPeerByPlayerId(PlayerId playerId) const noexcept
	{
		for (const auto& [_, peerState] : peerTable_)
		{
			if (peerState.isJoined && peerState.playerId == playerId)
			{
				return &peerState;
			}
		}

		return nullptr;
	}

	PeerState* PeerRoomManager::FindRecoverablePeerBySessionToken(const common::net::SessionToken& sessionToken) noexcept
	{
		if (!common::net::IsValidSessionToken(sessionToken))
		{
			return nullptr;
		}

		for (auto& [_, peerState] : peerTable_)
		{
			if (!peerState.isJoined)
			{
				continue;
			}

			if (peerState.connectionState != PeerConnectionState::Recoverable)
			{
				continue;
			}

			if (peerState.sessionToken == sessionToken)
			{
				return &peerState;
			}
		}

		return nullptr;
	}

	const PeerState* PeerRoomManager::FindRecoverablePeerBySessionToken(const common::net::SessionToken& sessionToken) const noexcept
	{
		if (!common::net::IsValidSessionToken(sessionToken))
		{
			return nullptr;
		}

		for (const auto& [_, peerState] : peerTable_)
		{
			if (!peerState.isJoined)
			{
				continue;
			}

			if (peerState.connectionState != PeerConnectionState::Recoverable)
			{
				continue;
			}

			if (peerState.sessionToken == sessionToken)
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

	PeerState& PeerRoomManager::UpsertJoinedPeer(const EndpointKey& endpointKey, PlayerId playerId, RoomId roomId, TimePoint currentTime)
	{
		PeerState& peerState = peerTable_[endpointKey];

		peerState.endpointKey = endpointKey;
		peerState.playerId = playerId;
		peerState.roomId = roomId;
		peerState.isJoined = true;

		peerState.connectionState = PeerConnectionState::Connected;
		peerState.lastRecvTime = currentTime;
		peerState.recoverableSince = {};

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

	bool PeerRoomManager::RefreshRecvTime(const EndpointKey& endpointKey, TimePoint currentTime) noexcept
	{
		PeerState* peerState = FindJoinedPeer(endpointKey);
		if (peerState == nullptr)
		{
			return false;
		}

		peerState->lastRecvTime = currentTime;
		return true;
	}

	bool PeerRoomManager::RebindRecoverablePeer(const EndpointKey& previousEndpointKey, const EndpointKey& nextEndpointKey, TimePoint currentTime)
	{
		PeerState* peerState = FindJoinedPeer(previousEndpointKey);
		if (peerState == nullptr)
		{
			return false;
		}

		if (peerState->connectionState != PeerConnectionState::Recoverable)
		{
			return false;
		}

		if (previousEndpointKey == nextEndpointKey)
		{
			peerState->connectionState = PeerConnectionState::Connected;
			peerState->lastRecvTime = currentTime;
			peerState->recoverableSince = {};
			return true;
		}

		if (peerTable_.contains(nextEndpointKey))
		{
			return false;
		}

		auto roomIterator = roomTable_.find(peerState->roomId);
		if (roomIterator == roomTable_.end())
		{
			return false;
		}

		RoomMemberSet& roomMemberSet = roomIterator->second;
		if (!roomMemberSet.contains(previousEndpointKey) || roomMemberSet.contains(nextEndpointKey))
		{
			return false;
		}

		PeerTable::node_type peerNode = peerTable_.extract(previousEndpointKey);
		if (peerNode.empty())
		{
			return false;
		}

		RoomMemberSet::node_type roomMemberNode = roomMemberSet.extract(previousEndpointKey);
		if (roomMemberNode.empty())
		{
			static_cast<void>(peerTable_.insert(std::move(peerNode)));
			return false;
		}

		peerNode.key() = nextEndpointKey;
		roomMemberNode.value() = nextEndpointKey;

		PeerState& reboundPeerState = peerNode.mapped();
		reboundPeerState.endpointKey = nextEndpointKey;
		reboundPeerState.connectionState = PeerConnectionState::Connected;
		reboundPeerState.lastRecvTime = currentTime;
		reboundPeerState.recoverableSince = {};

		const PeerTable::insert_return_type peerInsertResult = peerTable_.insert(std::move(peerNode));
		if (!peerInsertResult.inserted)
		{
			return false;
		}

		const RoomMemberSet::insert_return_type roomMemberInsertResult = roomMemberSet.insert(std::move(roomMemberNode));
		if (!roomMemberInsertResult.inserted)
		{
			return false;
		}

		return true;
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

	PeerRoomManager::RecoverablePeerList PeerRoomManager::MarkTimedOutPeersRecoverable(TimePoint currentTime, Duration timeout) noexcept
	{
		RecoverablePeerList recoverablePeerList;

		for (auto& [endpointKey, peerState] : peerTable_)
		{
			if (!peerState.isJoined)
			{
				continue;
			}

			if (peerState.connectionState != PeerConnectionState::Connected)
			{
				continue;
			}

			if (currentTime - peerState.lastRecvTime <= timeout)
			{
				continue;
			}

			peerState.connectionState = PeerConnectionState::Recoverable;
			peerState.recoverableSince = currentTime;

			recoverablePeerList.push_back(RecoverablePeer{
				.endpointKey = endpointKey,
				.playerId = peerState.playerId,
				.persistentPlayerId = peerState.persistentPlayerId,
				.roomId = peerState.roomId,
				});
		}

		return recoverablePeerList;
	}

	PeerRoomManager::ExpiredRecoverablePeerList PeerRoomManager::RemoveExpiredRecoverablePeers(TimePoint currentTime, Duration gracePeriod) noexcept
	{
		ExpiredRecoverablePeerList expiredPeerList;

		for (auto peerIterator = peerTable_.begin(); peerIterator != peerTable_.end();)
		{
			PeerState& peerState = peerIterator->second;
			if (!peerState.isJoined || peerState.connectionState != PeerConnectionState::Recoverable)
			{
				++peerIterator;
				continue;
			}

			if (currentTime - peerState.recoverableSince <= gracePeriod)
			{
				++peerIterator;
				continue;
			}

			expiredPeerList.push_back(ExpiredRecoverablePeer{
				.endpointKey = peerIterator->first,
				.playerId = peerState.playerId,
				.persistentPlayerId = peerState.persistentPlayerId,
				.roomId = peerState.roomId,
				});

			auto roomIterator = roomTable_.find(peerState.roomId);
			if (roomIterator != roomTable_.end())
			{
				roomIterator->second.erase(peerIterator->first);
				if (roomIterator->second.empty())
				{
					roomTable_.erase(roomIterator);
				}
			}

			peerIterator = peerTable_.erase(peerIterator);
		}

		return expiredPeerList;
	}

	PeerRoomManager::EndpointKeyList PeerRoomManager::BuildRoomEndpointKeyList(RoomId roomId) const
	{
		EndpointKeyList endpointKeyList;

		const RoomMemberSet* roomMemberSet = FindRoomMemberSet(roomId);
		if (roomMemberSet == nullptr)
		{
			return endpointKeyList;
		}

		endpointKeyList.reserve(roomMemberSet->size());

		for (const EndpointKey& endpointKey : *roomMemberSet)
		{
			const PeerState* peerState = FindJoinedPeer(endpointKey);
			if (peerState == nullptr)
			{
				continue;
			}

			if (peerState->connectionState != PeerConnectionState::Connected)
			{
				continue;
			}

			endpointKeyList.push_back(endpointKey);
		}

		return endpointKeyList;
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