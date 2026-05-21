#include "ThreadPoolTests.h"

#include <atomic>
#include <chrono>
#include <cstddef>

#include <Common/Diagnostics/DebugTestResult.h>
#include <Common/Threading/ThreadPool.h>

#include <Tests/TestHelpers.h>

namespace
{
	void RunStartWithZeroWorkerFailsTest(common::diagnostics::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(0);
		const bool started = startResult.has_value();

		common::diagnostics::Expect(result, !started, "ThreadPool: start with zero worker fails");
		common::diagnostics::Expect(result, !threadPool.IsRunning(), "ThreadPool: zero worker pool is not running");
		common::diagnostics::Expect(result, threadPool.GetWorkerThreadCount() == 0, "ThreadPool: zero worker count");
	}

	void RunStartAndStopTest(common::diagnostics::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(2);
		const bool started = startResult.has_value();

		common::diagnostics::Expect(result, started, "ThreadPool: start succeeds");
		common::diagnostics::Expect(result, threadPool.IsRunning(), "ThreadPool: running after start");
		common::diagnostics::Expect(result, threadPool.IsAcceptingTasks(), "ThreadPool: accepting tasks after start");
		common::diagnostics::Expect(result, threadPool.GetWorkerThreadCount() == 2, "ThreadPool: worker count after start");

		threadPool.Stop();

		common::diagnostics::Expect(result, !threadPool.IsRunning(), "ThreadPool: not running after stop");
		common::diagnostics::Expect(result, !threadPool.IsAcceptingTasks(), "ThreadPool: not accepting tasks after stop");
		common::diagnostics::Expect(result, threadPool.GetWorkerThreadCount() == 0, "ThreadPool: worker count after stop");
		common::diagnostics::Expect(result, threadPool.GetPendingTaskCount() == 0, "ThreadPool: pending task count after stop");
	}

	void RunStartTwiceFailsTest(common::diagnostics::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;

		const common::threading::ThreadPool::StartResult firstStartResult = threadPool.Start(1);
		const common::threading::ThreadPool::StartResult secondStartResult = threadPool.Start(1);

		const bool firstStarted = firstStartResult.has_value();
		const bool secondStarted = secondStartResult.has_value();

		common::diagnostics::Expect(result, firstStarted, "ThreadPool: first start succeeds");
		common::diagnostics::Expect(result, !secondStarted, "ThreadPool: second start fails");
		common::diagnostics::Expect(result, threadPool.GetWorkerThreadCount() == 1, "ThreadPool: worker count after second start");

		threadPool.Stop();
	}

	void RunEmptyTaskRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(1);
		const bool started = startResult.has_value();
		const bool enqueued = threadPool.Enqueue({});

		common::diagnostics::Expect(result, started, "ThreadPool: empty task test start succeeds");
		common::diagnostics::Expect(result, !enqueued, "ThreadPool: empty task rejected");

		threadPool.Stop();
	}

	void RunEnqueueAfterStopRejectedTest(common::diagnostics::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(1);
		const bool started = startResult.has_value();

		threadPool.Stop();

		const bool enqueued = threadPool.Enqueue(
			[]()
			{
			}
		);

		common::diagnostics::Expect(result, started, "ThreadPool: enqueue after stop start succeeds");
		common::diagnostics::Expect(result, !enqueued, "ThreadPool: enqueue after stop rejected");
	}

	void RunSingleTaskExecutesTest(common::diagnostics::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;
		std::atomic<int> executedCount = 0;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(1);
		const bool started = startResult.has_value();

		const bool enqueued = threadPool.Enqueue(
			[&executedCount]()
			{
				++executedCount;
			}
		);

		const bool completed = tests::WaitUntil(
			[&executedCount]()
			{
				return executedCount.load() == 1;
			},
			std::chrono::milliseconds(1000)
		);

		common::diagnostics::Expect(result, started, "ThreadPool: single task start succeeds");
		common::diagnostics::Expect(result, enqueued, "ThreadPool: single task enqueue succeeds");
		common::diagnostics::Expect(result, completed, "ThreadPool: single task completes");
		common::diagnostics::Expect(result, executedCount.load() == 1, "ThreadPool: single task executed count");

		threadPool.Stop();
	}

	void RunManyTasksExecuteTest(common::diagnostics::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;
		std::atomic<int> executedCount = 0;

		constexpr int taskCount = 128;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(4);
		const bool started = startResult.has_value();

		bool allEnqueued = true;

		for (int index = 0; index < taskCount; ++index)
		{
			allEnqueued = threadPool.Enqueue(
				[&executedCount]()
				{
					++executedCount;
				}
			) && allEnqueued;
		}

		const bool completed = tests::WaitUntil(
			[&executedCount]()
			{
				return executedCount.load() == taskCount;
			},
			std::chrono::milliseconds(1000)
		);

		common::diagnostics::Expect(result, started, "ThreadPool: many task start succeeds");
		common::diagnostics::Expect(result, allEnqueued, "ThreadPool: many task enqueue succeeds");
		common::diagnostics::Expect(result, completed, "ThreadPool: many task completes");
		common::diagnostics::Expect(result, executedCount.load() == taskCount, "ThreadPool: many task executed count");

		threadPool.Stop();
	}

	void RunTaskExceptionDoesNotStopWorkerTest(common::diagnostics::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;
		std::atomic<int> executedCount = 0;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(1);
		const bool started = startResult.has_value();

		const bool firstEnqueued = threadPool.Enqueue(
			[]()
			{
				throw 1;
			}
		);

		const bool secondEnqueued = threadPool.Enqueue(
			[&executedCount]()
			{
				++executedCount;
			}
		);

		const bool completed = tests::WaitUntil(
			[&executedCount]()
			{
				return executedCount.load() == 1;
			},
			std::chrono::milliseconds(1000)
		);

		common::diagnostics::Expect(result, started, "ThreadPool: exception task start succeeds");
		common::diagnostics::Expect(result, firstEnqueued, "ThreadPool: exception task enqueue succeeds");
		common::diagnostics::Expect(result, secondEnqueued, "ThreadPool: next task enqueue succeeds");
		common::diagnostics::Expect(result, completed, "ThreadPool: worker continues after exception");
		common::diagnostics::Expect(result, executedCount.load() == 1, "ThreadPool: post exception task executed");

		threadPool.Stop();
	}

	void RunStopAfterDrainExecutesPendingTasksTest(common::diagnostics::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;
		std::atomic<int> executedCount = 0;

		constexpr int taskCount = 64;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(2);
		const bool started = startResult.has_value();

		bool allEnqueued = true;

		for (int index = 0; index < taskCount; ++index)
		{
			allEnqueued = threadPool.Enqueue(
				[&executedCount]()
				{
					++executedCount;
				}
			) && allEnqueued;
		}

		threadPool.StopAfterDrain();

		common::diagnostics::Expect(result, started, "ThreadPool: drain start succeeds");
		common::diagnostics::Expect(result, allEnqueued, "ThreadPool: drain enqueue succeeds");
		common::diagnostics::Expect(result, executedCount.load() == taskCount, "ThreadPool: drain executes all tasks");
		common::diagnostics::Expect(result, !threadPool.IsRunning(), "ThreadPool: drain not running after stop");
		common::diagnostics::Expect(result, !threadPool.IsAcceptingTasks(), "ThreadPool: drain not accepting tasks");
		common::diagnostics::Expect(result, threadPool.GetWorkerThreadCount() == 0, "ThreadPool: drain worker count after stop");
		common::diagnostics::Expect(result, threadPool.GetPendingTaskCount() == 0, "ThreadPool: drain pending count after stop");
	}

	void RunStopAfterDrainRejectsNewTasksTest(common::diagnostics::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(1);
		const bool started = startResult.has_value();

		threadPool.StopAfterDrain();

		const bool enqueued = threadPool.Enqueue(
			[]()
			{
			}
		);

		common::diagnostics::Expect(result, started, "ThreadPool: drain reject start succeeds");
		common::diagnostics::Expect(result, !enqueued, "ThreadPool: drain rejects new task after stop");
	}
}

namespace tests::threading
{
	common::diagnostics::DebugTestResult RunThreadPoolTests()
	{
		common::diagnostics::DebugTestResult result{};

		RunStartWithZeroWorkerFailsTest(result);
		RunStartAndStopTest(result);
		RunStartTwiceFailsTest(result);
		RunEmptyTaskRejectedTest(result);
		RunEnqueueAfterStopRejectedTest(result);
		RunSingleTaskExecutesTest(result);
		RunManyTasksExecuteTest(result);
		RunStopAfterDrainExecutesPendingTasksTest(result);
		RunStopAfterDrainRejectsNewTasksTest(result);
		RunTaskExceptionDoesNotStopWorkerTest(result);

		return result;
	}
}