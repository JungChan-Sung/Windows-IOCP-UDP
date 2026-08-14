#pragma once

#include <cstddef>
#include <mutex>
#include <queue>
#include <vector>

#include <Common/Threading/ThreadPool.h>

#include <Persistence/Core/PersistenceRuntime.h>

#include <Server/Game/MatchHistoryTracker.h>

namespace server::match
{
	struct MatchHistorySaveCompletion
	{
	public:
		common::game::RoomId roomId = 0;
		persistence::PersistenceRuntime::SaveMatchResult saveResult;
	};

	class MatchHistoryTaskProcessor final
	{
	public:
		using StartResult = common::threading::ThreadPool::StartResult;
		using CompletionList = std::vector<MatchHistorySaveCompletion>;

	private:
		persistence::PersistenceRuntime& persistenceRuntime_;

		common::threading::ThreadPool workerPool_;

		std::mutex completionMutex_;
		std::queue<MatchHistorySaveCompletion> completionQueue_;

	public:
		explicit MatchHistoryTaskProcessor(persistence::PersistenceRuntime& persistenceRuntime) noexcept;
		~MatchHistoryTaskProcessor() noexcept;

		MatchHistoryTaskProcessor(const MatchHistoryTaskProcessor&) = delete;
		MatchHistoryTaskProcessor& operator=(const MatchHistoryTaskProcessor&) = delete;

		MatchHistoryTaskProcessor(MatchHistoryTaskProcessor&&) = delete;
		MatchHistoryTaskProcessor& operator=(MatchHistoryTaskProcessor&&) = delete;

	public:
		[[nodiscard]] StartResult Start(std::size_t workerThreadCount = 1);

		void Stop() noexcept;
		void StopAfterDrain() noexcept;

		[[nodiscard]] bool Enqueue(game::CompletedMatch completedMatch);
		[[nodiscard]] CompletionList ExtractCompletionList();

	private:
		void ClearCompletions() noexcept;
	};
}