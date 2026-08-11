#pragma once

#include <WinSock2.h>

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <unordered_map>
#include <vector>

#include <Common/Net/Endpoint.h>
#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Time/TimeTypes.h>

#include <Server/Account/AccountLoginTaskProcessor.h>

namespace server::net
{
	class AccountLoginPacketHandler final
	{
	public:
		using TimePoint = common::time::TimePoint;
		using Duration = common::time::Duration;

		using TaskId = account::AccountLoginTaskId;

	public:
		enum class EnqueueStatus
		{
			Enqueued,
			DuplicatePending,
			CachedResponseQueued,
			TaskEnqueueFailed,
		};

		struct ResponseTask
		{
		public:
			sockaddr_in remoteAddress{};
			common::packet::AccountLoginResponsePacket responsePacket;
			std::int64_t persistentPlayerId = 0;
			TaskId taskId = invalidTaskId;
			bool isLatestRequest = false;
		};
		 
	private:
		struct RequestKey
		{
		public:
			common::net::EndpointKey endpointKey{};
			common::packet::AccountLoginRequestId requestId = common::packet::invalidAccountLoginRequestId;

		public:
			bool operator==(const RequestKey& other) const = default;
		};

		struct RequestKeyHasher
		{
			[[nodiscard]] std::size_t operator()(const RequestKey& key) const noexcept
			{
				const std::size_t endpointHash = common::net::EndpointKeyHasher{}(key.endpointKey);
				const std::size_t requestIdHash = std::hash<common::packet::AccountLoginRequestId>{}(key.requestId);

				return endpointHash ^ (requestIdHash + (endpointHash << 6) + (endpointHash >> 2));
			}
		};

		struct PendingRequest
		{
		public:
			sockaddr_in remoteAddress{};
			RequestKey requestKey{};
		};

		struct LatestRequest
		{
		public:
			TaskId taskId = invalidTaskId;
			TimePoint updatedTime{};
		};

		struct CachedResponse
		{
		public:
			common::packet::AccountLoginResponsePacket responsePacket{};
			TimePoint cachedTime{};
		};

	public:
		using ResponseTaskList = std::vector<ResponseTask>;

	private:
		using PendingRequestTable = std::unordered_map<TaskId, PendingRequest>;
		using PendingTaskTable = std::unordered_map<RequestKey, TaskId, RequestKeyHasher>;
		using ResponseCache = std::unordered_map<RequestKey, CachedResponse, RequestKeyHasher>;
		using LatestRequestTable = std::unordered_map<common::net::EndpointKey, LatestRequest, common::net::EndpointKeyHasher>;

	public:
		static inline constexpr TaskId invalidTaskId = 0;

	private:
		static inline constexpr Duration responseCacheLifetime = common::time::Seconds(10);

	private:
		account::AccountLoginTaskProcessor& taskProcessor_;

		std::atomic<TaskId> nextTaskId_ = 1;

		mutable std::mutex stateMutex_;

		PendingRequestTable pendingRequestTable_;
		PendingTaskTable pendingTaskTable_;
		ResponseCache responseCache_;
		LatestRequestTable latestRequestTable_;

		std::queue<ResponseTask> readyResponseQueue_;

	public:
		explicit AccountLoginPacketHandler(account::AccountLoginTaskProcessor& taskProcessor) noexcept;
		~AccountLoginPacketHandler() noexcept = default;

		AccountLoginPacketHandler(const AccountLoginPacketHandler&) = delete;
		AccountLoginPacketHandler& operator=(const AccountLoginPacketHandler&) = delete;

		AccountLoginPacketHandler(AccountLoginPacketHandler&&) = delete;
		AccountLoginPacketHandler& operator=(AccountLoginPacketHandler&&) = delete;

	public:
		[[nodiscard]] EnqueueStatus Enqueue(const sockaddr_in& remoteAddress, const common::packet::AccountLoginRequestPacket& packet, TimePoint currentTime);

		[[nodiscard]] ResponseTaskList ExtractResponseTaskList(TimePoint currentTime);

		[[nodiscard]] bool FinalizeResponse(TaskId taskId, const common::packet::AccountLoginResponsePacket& responsePacket, TimePoint currentTime);

		void Clear() noexcept;

	private:
		void RemoveExpiredCachedResponsesLocked(TimePoint currentTime) noexcept;
		void RemoveExpiredLatestRequestsLocked(TimePoint currentTime) noexcept;

	public:
		[[nodiscard]] std::size_t GetPendingRequestCount() const;
	};
}