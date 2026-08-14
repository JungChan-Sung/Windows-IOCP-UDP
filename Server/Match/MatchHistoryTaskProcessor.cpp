#include "MatchHistoryTaskProcessor.h"

#include <utility>
#include <vector>

namespace server::match
{
	MatchHistoryTaskProcessor::MatchHistoryTaskProcessor(persistence::PersistenceRuntime& persistenceRuntime) noexcept
		: persistenceRuntime_(persistenceRuntime)
	{}

	MatchHistoryTaskProcessor::~MatchHistoryTaskProcessor() noexcept
	{
		Stop();
	}

	MatchHistoryTaskProcessor::StartResult MatchHistoryTaskProcessor::Start(std::size_t workerThreadCount)
	{
		ClearCompletions();
		return workerPool_.Start(workerThreadCount);
	}

	void MatchHistoryTaskProcessor::Stop() noexcept
	{
		workerPool_.Stop();
		ClearCompletions();
	}

	void MatchHistoryTaskProcessor::StopAfterDrain() noexcept
	{
		workerPool_.StopAfterDrain();
	}

	bool MatchHistoryTaskProcessor::Enqueue(game::CompletedMatch completedMatch)
	{
		return workerPool_.Enqueue(
			[this, completedMatch = std::move(completedMatch)]()
			{
				std::vector<persistence::match::MatchPlayerCreateRecord> playerStatsList;
				playerStatsList.reserve(completedMatch.playerStatsList.size());

				for (const game::MatchPlayerStats& playerStats : completedMatch.playerStatsList)
				{
					playerStatsList.push_back(persistence::match::MatchPlayerCreateRecord{
						.persistentPlayerId = playerStats.persistentPlayerId,
						.killCount = playerStats.killCount,
						.deathCount = playerStats.deathCount,
						});
				}

				persistence::PersistenceRuntime::SaveMatchResult saveResult = persistenceRuntime_.SaveMatch(persistence::match::MatchCreateRequest{
						.roomId = completedMatch.roomId,
						.startedAt = completedMatch.startedAt,
						.endedAt = completedMatch.endedAt,
						.playerStatsList = playerStatsList,
					});

				MatchHistorySaveCompletion completion{
					.roomId = completedMatch.roomId,
					.saveResult = std::move(saveResult),
				};

				std::scoped_lock lock(completionMutex_);
				completionQueue_.push(std::move(completion));
			}
		);
	}

	MatchHistoryTaskProcessor::CompletionList MatchHistoryTaskProcessor::ExtractCompletionList()
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

	void MatchHistoryTaskProcessor::ClearCompletions() noexcept
	{
		std::scoped_lock lock(completionMutex_);

		std::queue<MatchHistorySaveCompletion> emptyQueue;
		completionQueue_.swap(emptyQueue);
	}
}