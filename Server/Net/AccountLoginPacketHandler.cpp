#include "AccountLoginPacketHandler.h"

#include <utility>

#include <Server/Net/AccountPacketMapper.h>

namespace server::net
{
	AccountLoginPacketHandler::AccountLoginPacketHandler(account::AccountLoginTaskProcessor& taskProcessor) noexcept
		: taskProcessor_(taskProcessor)
	{}

	AccountLoginPacketHandler::EnqueueStatus AccountLoginPacketHandler::Enqueue(const sockaddr_in& remoteAddress, const common::packet::AccountLoginRequestPacket& packet, TimePoint currentTime)
	{
		const RequestKey requestKey{
			.endpointKey = common::net::MakeEndpointKey(remoteAddress),
			.requestId = packet.requestId,
		};

		TaskId taskId = 0;

		{
			std::scoped_lock lock(stateMutex_);

			RemoveExpiredCachedResponsesLocked(currentTime);

			const auto cachedResponseIterator = responseCache_.find(requestKey);
			if (cachedResponseIterator != responseCache_.end())
			{
				readyResponseQueue_.push(ResponseTask{
						.remoteAddress = remoteAddress,
						.responsePacket = cachedResponseIterator->second.responsePacket,
						.taskId = invalidTaskId,
					});

				return EnqueueStatus::CachedResponseQueued;
			}

			if (pendingTaskTable_.contains(requestKey))
			{
				return EnqueueStatus::DuplicatePending;
			}

			taskId = nextTaskId_.fetch_add(1, std::memory_order_relaxed);
			const bool pendingRequestInserted = pendingRequestTable_.emplace(
				taskId,
				PendingRequest{
					.remoteAddress = remoteAddress,
					.requestKey = requestKey,
				}).second;

			const bool pendingTaskInserted = pendingTaskTable_.emplace(requestKey, taskId).second;
			if (!pendingRequestInserted || !pendingTaskInserted)
			{
				pendingRequestTable_.erase(taskId);
				pendingTaskTable_.erase(requestKey);

				return EnqueueStatus::TaskEnqueueFailed;
			}
		}

		account::AccountLoginTask task{
			.taskId = taskId,
			.loginName = packet.loginName,
			.passwordHash = packet.passwordHash,
		};
		if (taskProcessor_.Enqueue(std::move(task)))
		{
			return EnqueueStatus::Enqueued;
		}

		{
			std::scoped_lock lock(stateMutex_);

			pendingRequestTable_.erase(taskId);
			pendingTaskTable_.erase(requestKey);
		}

		return EnqueueStatus::TaskEnqueueFailed;
	}

	AccountLoginPacketHandler::ResponseTaskList AccountLoginPacketHandler::ExtractResponseTaskList(TimePoint currentTime)
	{
		account::AccountLoginTaskProcessor::CompletionList completionList = taskProcessor_.ExtractCompletionList();

		ResponseTaskList responseTaskList;

		{
			std::scoped_lock lock(stateMutex_);

			RemoveExpiredCachedResponsesLocked(currentTime);

			responseTaskList.reserve(readyResponseQueue_.size() + completionList.size());

			while (!readyResponseQueue_.empty())
			{
				responseTaskList.push_back(std::move(readyResponseQueue_.front()));
				readyResponseQueue_.pop();
			}

			for (account::AccountLoginCompletion& completion : completionList)
			{
				const auto pendingRequestIterator = pendingRequestTable_.find(completion.taskId);
				if (pendingRequestIterator == pendingRequestTable_.end())
				{
					continue;
				}

				PendingRequest pendingRequest = std::move(pendingRequestIterator->second);

				responseTaskList.push_back(ResponseTask{
						.remoteAddress = pendingRequest.remoteAddress,
						.responsePacket = BuildAccountLoginResponse(pendingRequest.requestKey.requestId, std::move(completion.loginResult)),
						.taskId = completion.taskId,
					});
			}
		}

		return responseTaskList;
	}

	bool AccountLoginPacketHandler::FinalizeResponse(TaskId taskId, const common::packet::AccountLoginResponsePacket& responsePacket, TimePoint currentTime)
	{
		if (taskId == invalidTaskId)
		{
			return false;
		}

		std::scoped_lock lock(stateMutex_);

		RemoveExpiredCachedResponsesLocked(currentTime);

		const auto pendingRequestIterator = pendingRequestTable_.find(taskId);
		if (pendingRequestIterator == pendingRequestTable_.end())
		{
			return false;
		}

		const PendingRequest& pendingRequest = pendingRequestIterator->second;
		if (responsePacket.requestId != pendingRequest.requestKey.requestId)
		{
			return false;
		}

		responseCache_.insert_or_assign(
			pendingRequest.requestKey,
			CachedResponse{
				.responsePacket = responsePacket,
				.cachedTime = currentTime,
			});

		pendingTaskTable_.erase(pendingRequest.requestKey);
		pendingRequestTable_.erase(pendingRequestIterator);

		return true;
	}

	void AccountLoginPacketHandler::Clear()
	{
		std::scoped_lock lock(stateMutex_);

		pendingRequestTable_.clear();
		pendingTaskTable_.clear();
		responseCache_.clear();

		while (!readyResponseQueue_.empty())
		{
			readyResponseQueue_.pop();
		}
	}

	void AccountLoginPacketHandler::RemoveExpiredCachedResponsesLocked(TimePoint currentTime) noexcept
	{
		for (auto iterator = responseCache_.begin(); iterator != responseCache_.end();)
		{
			const CachedResponse& cachedResponse = iterator->second;
			if (currentTime - cachedResponse.cachedTime < responseCacheLifetime)
			{
				++iterator;
				continue;
			}

			iterator = responseCache_.erase(iterator);
		}
	}

	std::size_t AccountLoginPacketHandler::GetPendingRequestCount() const
	{
		std::scoped_lock lock(stateMutex_);
		return pendingRequestTable_.size();
	}
}