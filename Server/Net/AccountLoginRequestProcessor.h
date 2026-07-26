#pragma once

#include <WinSock2.h>

#include <cstddef>
#include <mutex>
#include <queue>
#include <vector>

#include <Common/Packet/Account/AccountPacket.h>
#include <Common/Threading/ThreadPool.h>

#include <Server/Account/AccountService.h>

namespace server::net
{
	class AccountLoginRequestProcessor final
	{
	public:
		struct ResponseTask
		{
		public:
			sockaddr_in remoteAddress{};
			common::packet::AccountLoginResponsePacket responsePacket;
		};

	public:
		using StartResult = common::threading::ThreadPool::StartResult;
		using ResponseTaskList = std::vector<ResponseTask>;

	private:
		server::account::AccountService& accountService_;

		common::threading::ThreadPool workerPool_;

		std::mutex responseMutex_;
		std::queue<ResponseTask> responseTaskQueue_;

	public:
		explicit AccountLoginRequestProcessor(server::account::AccountService& accountService) noexcept;
		~AccountLoginRequestProcessor() noexcept;

		AccountLoginRequestProcessor(const AccountLoginRequestProcessor&) = delete;
		AccountLoginRequestProcessor& operator=(const AccountLoginRequestProcessor&) = delete;

		AccountLoginRequestProcessor(AccountLoginRequestProcessor&&) = delete;
		AccountLoginRequestProcessor& operator=(AccountLoginRequestProcessor&&) = delete;

	public:
		[[nodiscard]] StartResult Start(std::size_t workerThreadCount = 1);

		void Stop() noexcept;
		void StopAfterDrain() noexcept;

		[[nodiscard]] bool Enqueue(const sockaddr_in& remoteAddress, const common::packet::AccountLoginRequestPacket& packet);

		[[nodiscard]] ResponseTaskList ExtractResponseTaskList();

	private:
		void ClearResponseTasks() noexcept;
	};
}