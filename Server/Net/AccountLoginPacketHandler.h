#pragma once

#include <WinSock2.h>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <Common/Packet/Account/AccountPacket.h>

#include <Server/Account/AccountLoginTaskProcessor.h>

namespace server::net
{
	class AccountLoginPacketHandler final
	{
	public:
		struct ResponseTask
		{
		public:
			sockaddr_in remoteAddress{};
			common::packet::AccountLoginResponsePacket responsePacket;
		};
		 
	private:
		struct PendingRequest
		{
		public:
			sockaddr_in remoteAddress{};
			common::packet::AccountLoginRequestId requestId = common::packet::invalidAccountLoginRequestId;
		};

	public:
		using ResponseTaskList = std::vector<ResponseTask>;

	private:
		using TaskId = account::AccountLoginTaskId;
		using PendingRequestTable = std::unordered_map<TaskId, PendingRequest>;

	private:
		account::AccountLoginTaskProcessor& taskProcessor_;

		std::atomic<TaskId> nextTaskId_ = 1;

		mutable std::mutex pendingRequestMutex_;
		PendingRequestTable pendingRequestTable_;

	public:
		explicit AccountLoginPacketHandler(account::AccountLoginTaskProcessor& taskProcessor) noexcept;
		~AccountLoginPacketHandler() noexcept = default;

		AccountLoginPacketHandler(const AccountLoginPacketHandler&) = delete;
		AccountLoginPacketHandler& operator=(const AccountLoginPacketHandler&) = delete;

		AccountLoginPacketHandler(AccountLoginPacketHandler&&) = delete;
		AccountLoginPacketHandler& operator=(AccountLoginPacketHandler&&) = delete;

	public:
		[[nodiscard]] bool Enqueue(const sockaddr_in& remoteAddress, const common::packet::AccountLoginRequestPacket& packet);

		[[nodiscard]] ResponseTaskList ExtractResponseTaskList();

		void ClearPendingRequests() noexcept;

	private:
		[[nodiscard]] std::optional<PendingRequest> TakePendingRequest(TaskId taskId);

	public:
		[[nodiscard]] std::size_t GetPendingRequestCount() const;
	};
}