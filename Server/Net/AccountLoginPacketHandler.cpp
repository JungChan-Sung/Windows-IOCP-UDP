#include "AccountLoginPacketHandler.h"

#include <utility>

#include <Server/Net/AccountPacketMapper.h>

namespace server::net
{
	AccountLoginPacketHandler::AccountLoginPacketHandler(server::account::AccountLoginTaskProcessor& taskProcessor) noexcept
		: taskProcessor_(taskProcessor)
	{}

	bool AccountLoginPacketHandler::Enqueue(const sockaddr_in& remoteAddress, const common::packet::AccountLoginRequestPacket& packet)
	{
		const TaskId taskId = nextTaskId_.fetch_add(1, std::memory_order_relaxed);

		{
			std::scoped_lock lock(pendingEndpointMutex_);

			const bool inserted = pendingEndpointTable_.emplace(taskId, remoteAddress).second;
			if (!inserted)
			{
				return false;
			}
		}

		server::account::AccountLoginTask task{
			.taskId = taskId,
			.loginName = packet.loginName,
			.passwordHash = packet.passwordHash,
		};

		if (taskProcessor_.Enqueue(std::move(task)))
		{
			return true;
		}

		{
			std::scoped_lock lock(pendingEndpointMutex_);
			pendingEndpointTable_.erase(taskId);
		}

		return false;
	}

	AccountLoginPacketHandler::ResponseTaskList AccountLoginPacketHandler::ExtractResponseTaskList()
	{
		server::account::AccountLoginTaskProcessor::CompletionList completionList = taskProcessor_.ExtractCompletionList();

		ResponseTaskList responseTaskList;
		responseTaskList.reserve(completionList.size());

		for (server::account::AccountLoginCompletion& completion : completionList)
		{
			const std::optional<sockaddr_in> remoteAddress = TakeRemoteAddress(completion.taskId);
			if (!remoteAddress.has_value())
			{
				continue;
			}

			responseTaskList.push_back(ResponseTask{
					.remoteAddress = *remoteAddress,
					.responsePacket = BuildAccountLoginResponse(std::move(completion.loginResult)),
				});
		}

		return responseTaskList;
	}

	void AccountLoginPacketHandler::ClearPendingRequests() noexcept
	{
		std::scoped_lock lock(pendingEndpointMutex_);
		pendingEndpointTable_.clear();
	}

	std::optional<sockaddr_in> AccountLoginPacketHandler::TakeRemoteAddress(TaskId taskId)
	{
		std::scoped_lock lock(pendingEndpointMutex_);

		const auto iterator = pendingEndpointTable_.find(taskId);
		if (iterator == pendingEndpointTable_.end())
		{
			return std::nullopt;
		}

		const sockaddr_in remoteAddress = iterator->second;
		pendingEndpointTable_.erase(iterator);

		return remoteAddress;
	}

	std::size_t AccountLoginPacketHandler::GetPendingRequestCount() const
	{
		std::scoped_lock lock(pendingEndpointMutex_);
		return pendingEndpointTable_.size();
	}
}