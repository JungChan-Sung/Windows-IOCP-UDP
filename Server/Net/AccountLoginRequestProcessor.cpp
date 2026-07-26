#include "AccountLoginRequestProcessor.h"

#include <string>
#include <utility>

#include <Server/Account/AccountPacketMapper.h>

namespace server::net
{
	AccountLoginRequestProcessor::AccountLoginRequestProcessor(server::account::AccountService& accountService) noexcept
		: accountService_(accountService)
	{}

	AccountLoginRequestProcessor::~AccountLoginRequestProcessor() noexcept
	{
		Stop();
	}

	AccountLoginRequestProcessor::StartResult AccountLoginRequestProcessor::Start(std::size_t workerThreadCount)
	{
		ClearResponseTasks();
		return workerPool_.Start(workerThreadCount);
	}

	void AccountLoginRequestProcessor::Stop() noexcept
	{
		workerPool_.Stop();
		ClearResponseTasks();
	}

	void AccountLoginRequestProcessor::StopAfterDrain() noexcept
	{
		workerPool_.StopAfterDrain();
	}

	bool AccountLoginRequestProcessor::Enqueue(const sockaddr_in& remoteAddress, const common::packet::AccountLoginRequestPacket& packet)
	{
		std::string loginName = packet.loginName;
		std::string passwordHash = packet.passwordHash;

		return workerPool_.Enqueue(
			[this, remoteAddress, loginName = std::move(loginName), passwordHash = std::move(passwordHash)]()
			{
				server::account::LoginAccountResult loginResult = accountService_.LoginAccount(server::account::AccountLoginRequest{
						.loginName = loginName,
						.passwordHash = passwordHash,
					});

				ResponseTask responseTask{
					.remoteAddress = remoteAddress,
					.responsePacket = server::account::BuildAccountLoginResponse(std::move(loginResult)),
				};

				std::scoped_lock lock(responseMutex_);
				responseTaskQueue_.push(std::move(responseTask));
			}
		);
	}

	AccountLoginRequestProcessor::ResponseTaskList AccountLoginRequestProcessor::ExtractResponseTaskList()
	{
		ResponseTaskList responseTaskList;

		std::scoped_lock lock(responseMutex_);

		responseTaskList.reserve(responseTaskQueue_.size());

		while (!responseTaskQueue_.empty())
		{
			responseTaskList.push_back(std::move(responseTaskQueue_.front()));
			responseTaskQueue_.pop();
		}

		return responseTaskList;
	}

	void AccountLoginRequestProcessor::ClearResponseTasks() noexcept
	{
		std::scoped_lock lock(responseMutex_);

		std::queue<ResponseTask> emptyQueue;
		responseTaskQueue_.swap(emptyQueue);
	}
}