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
			std::scoped_lock lock(pendingRequestMutex_);

			const bool inserted = pendingRequestTable_.emplace(
				taskId,
				PendingRequest{ 
					.remoteAddress = remoteAddress,
					.requestId = packet.requestId,
				}).second;
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
			std::scoped_lock lock(pendingRequestMutex_);
			pendingRequestTable_.erase(taskId);
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
			const std::optional<PendingRequest> pendingRequest = TakePendingRequest(completion.taskId);
			if (!pendingRequest.has_value())
			{
				continue;
			}

			responseTaskList.push_back(
				ResponseTask{
					.remoteAddress = pendingRequest->remoteAddress,
					.responsePacket = BuildAccountLoginResponse(pendingRequest->requestId, std::move(completion.loginResult)),
				}
			);
		}

		return responseTaskList;
	}

	void AccountLoginPacketHandler::ClearPendingRequests() noexcept
	{
		std::scoped_lock lock(pendingRequestMutex_);
		pendingRequestTable_.clear();
	}

	std::optional<AccountLoginPacketHandler::PendingRequest> AccountLoginPacketHandler::TakePendingRequest(TaskId taskId)
	{
		std::scoped_lock lock(pendingRequestMutex_);

		const auto iterator = pendingRequestTable_.find(taskId);
		if (iterator == pendingRequestTable_.end())
		{
			return std::nullopt;
		}

		PendingRequest pendingRequest = iterator->second;
		pendingRequestTable_.erase(iterator);

		return pendingRequest;
	}

	std::size_t AccountLoginPacketHandler::GetPendingRequestCount() const
	{
		std::scoped_lock lock(pendingRequestMutex_);
		return pendingRequestTable_.size();
	}
}