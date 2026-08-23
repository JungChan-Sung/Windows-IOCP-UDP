#include "AccountLoginPacketHandler.h"

#include <optional>
#include <utility>

#include <Server/Protocol/AccountPacketMapper.h>

namespace server::protocol
{
	AccountLoginPacketHandler::AccountLoginPacketHandler(account::AccountLoginTaskProcessor& taskProcessor) noexcept
		: taskProcessor_(taskProcessor)
	{}

	AccountLoginPacketHandler::EnqueueStatus AccountLoginPacketHandler::Enqueue(const common::net::EndpointKey& endpointKey, const common::packet::AccountLoginRequestPacket& packet, TimePoint currentTime)
	{
		const RequestKey requestKey{
			.endpointKey = endpointKey,
			.requestId = packet.requestId,
		};

		TaskId taskId = invalidTaskId;
		std::optional<LatestRequest> previousLatestRequest;

		{
			std::scoped_lock lock(stateMutex_);

			RemoveExpiredCachedResponsesLocked(currentTime);
			RemoveExpiredLatestRequestsLocked(currentTime);

			const auto cachedResponseIterator = responseCache_.find(requestKey);
			if (cachedResponseIterator != responseCache_.end())
			{
				readyResponseQueue_.push(ResponseTask{
						.endpointKey = endpointKey,
						.responsePacket = cachedResponseIterator->second.responsePacket,
						.taskId = invalidTaskId,
						.isLatestRequest = false,
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
					.requestKey = requestKey,
				}).second;
			const bool pendingTaskInserted = pendingTaskTable_.emplace(requestKey, taskId).second;
			if (!pendingRequestInserted || !pendingTaskInserted)
			{
				pendingRequestTable_.erase(taskId);
				pendingTaskTable_.erase(requestKey);

				return EnqueueStatus::TaskEnqueueFailed;
			}

			const auto latestRequestIterator = latestRequestTable_.find(endpointKey);
			if (latestRequestIterator != latestRequestTable_.end())
			{
				previousLatestRequest = latestRequestIterator->second;
			}

			latestRequestTable_.insert_or_assign(endpointKey, LatestRequest{
				.taskId = taskId,
				.updatedTime = currentTime,
				});
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

			const auto latestRequestIterator = latestRequestTable_.find(endpointKey);
			if (latestRequestIterator != latestRequestTable_.end() && latestRequestIterator->second.taskId == taskId)
			{
				if (previousLatestRequest.has_value())
				{
					latestRequestIterator->second = *previousLatestRequest;
				}
				else
				{
					latestRequestTable_.erase(latestRequestIterator);
				}
			}
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
			RemoveExpiredLatestRequestsLocked(currentTime);

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

				const PendingRequest& pendingRequest = pendingRequestIterator->second;
				const auto latestRequestIterator = latestRequestTable_.find(pendingRequest.requestKey.endpointKey);
				const bool isLatestRequest = (latestRequestIterator != latestRequestTable_.end()) && (latestRequestIterator->second.taskId == completion.taskId);
				common::packet::AccountLoginResponsePacket responsePacket = BuildAccountLoginResponse(pendingRequest.requestKey.requestId, completion.loginResult);

				std::optional<account::AccountLoginRecord> accountLoginRecord;
				if (completion.loginResult.has_value())
				{
					accountLoginRecord = std::move(*completion.loginResult);
				}

				responseTaskList.push_back(ResponseTask{
					.endpointKey = pendingRequest.requestKey.endpointKey,
					.responsePacket = std::move(responsePacket),
					.accountLoginRecord = std::move(accountLoginRecord),
					.taskId = completion.taskId,
					.isLatestRequest = isLatestRequest,
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
		RemoveExpiredLatestRequestsLocked(currentTime);

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

		const auto latestRequestIterator = latestRequestTable_.find(pendingRequest.requestKey.endpointKey);
		if ((latestRequestIterator != latestRequestTable_.end()) && (latestRequestIterator->second.taskId == taskId))
		{
			latestRequestIterator->second.updatedTime = currentTime;
		}

		pendingTaskTable_.erase(pendingRequest.requestKey);
		pendingRequestTable_.erase(pendingRequestIterator);

		return true;
	}

	void AccountLoginPacketHandler::Clear() noexcept
	{
		std::scoped_lock lock(stateMutex_);

		pendingRequestTable_.clear();
		pendingTaskTable_.clear();
		responseCache_.clear();
		latestRequestTable_.clear();

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

	void AccountLoginPacketHandler::RemoveExpiredLatestRequestsLocked(TimePoint currentTime) noexcept
	{
		for (auto iterator = latestRequestTable_.begin(); iterator != latestRequestTable_.end();)
		{
			const LatestRequest& latestRequest = iterator->second;
			if (pendingRequestTable_.contains(latestRequest.taskId))
			{
				++iterator;
				continue;
			}

			if (currentTime - latestRequest.updatedTime < responseCacheLifetime)
			{
				++iterator;
				continue;
			}

			iterator = latestRequestTable_.erase(iterator);
		}
	}

	std::size_t AccountLoginPacketHandler::GetPendingRequestCount() const
	{
		std::scoped_lock lock(stateMutex_);
		return pendingRequestTable_.size();
	}
}