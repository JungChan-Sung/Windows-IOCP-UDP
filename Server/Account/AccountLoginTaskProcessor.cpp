#include "AccountLoginTaskProcessor.h"

#include <utility>

namespace server::account
{
	AccountLoginTaskProcessor::AccountLoginTaskProcessor(AccountService& accountService) noexcept
		: accountService_(accountService)
	{}

	AccountLoginTaskProcessor::~AccountLoginTaskProcessor() noexcept
	{
		Stop();
	}

	AccountLoginTaskProcessor::StartResult AccountLoginTaskProcessor::Start(std::size_t workerThreadCount)
	{
		ClearCompletions();
		return workerPool_.Start(workerThreadCount);
	}

	void AccountLoginTaskProcessor::Stop() noexcept
	{
		workerPool_.Stop();
		ClearCompletions();
	}

	void AccountLoginTaskProcessor::StopAfterDrain() noexcept
	{
		workerPool_.StopAfterDrain();
	}

	bool AccountLoginTaskProcessor::Enqueue(AccountLoginTask task)
	{
		return workerPool_.Enqueue(
			[this, task = std::move(task)]()
			{
				LoginAccountResult loginResult = accountService_.LoginAccount(AccountLoginRequest{
						.loginName = task.loginName,
						.passwordHash = task.passwordHash,
					});

				AccountLoginCompletion completion{
					.taskId = task.taskId,
					.loginResult = std::move(loginResult),
				};

				std::scoped_lock lock(completionMutex_);
				completionQueue_.push(std::move(completion));
			}
		);
	}

	AccountLoginTaskProcessor::CompletionList AccountLoginTaskProcessor::ExtractCompletionList()
	{
		CompletionList completionList;

		std::scoped_lock lock(completionMutex_);

		completionList.reserve(completionQueue_.size());

		while (!completionQueue_.empty())
		{
			completionList.push_back(std::move(completionQueue_.front()));
			completionQueue_.pop();
		}

		return completionList;
	}

	void AccountLoginTaskProcessor::ClearCompletions() noexcept
	{
		std::scoped_lock lock(completionMutex_);

		std::queue<AccountLoginCompletion> emptyQueue;
		completionQueue_.swap(emptyQueue);
	}
}