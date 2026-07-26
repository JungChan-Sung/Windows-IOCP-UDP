#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include <Common/Threading/ThreadPool.h>

#include <Server/Account/AccountService.h>

namespace server::account
{
	using AccountLoginTaskId = std::uint64_t;

	struct AccountLoginTask
	{
	public:
		AccountLoginTaskId taskId = 0;
		std::string loginName;
		std::string passwordHash;
	};

	struct AccountLoginCompletion
	{
	public:
		AccountLoginTaskId taskId = 0;
		LoginAccountResult loginResult;
	};

	class AccountLoginTaskProcessor final
	{
	public:
		using StartResult = common::threading::ThreadPool::StartResult;
		using CompletionList = std::vector<AccountLoginCompletion>;

	private:
		AccountService& accountService_;

		common::threading::ThreadPool workerPool_;

		std::mutex completionMutex_;
		std::queue<AccountLoginCompletion> completionQueue_;

	public:
		explicit AccountLoginTaskProcessor(AccountService& accountService) noexcept;
		~AccountLoginTaskProcessor() noexcept;

		AccountLoginTaskProcessor(const AccountLoginTaskProcessor&) = delete;
		AccountLoginTaskProcessor& operator=(const AccountLoginTaskProcessor&) = delete;

		AccountLoginTaskProcessor(AccountLoginTaskProcessor&&) = delete;
		AccountLoginTaskProcessor& operator=(AccountLoginTaskProcessor&&) = delete;

	public:
		[[nodiscard]] StartResult Start(std::size_t workerThreadCount = 1);

		void Stop() noexcept;
		void StopAfterDrain() noexcept;

		[[nodiscard]] bool Enqueue(AccountLoginTask task);
		[[nodiscard]] CompletionList ExtractCompletionList();

	private:
		void ClearCompletions() noexcept;
	};
}