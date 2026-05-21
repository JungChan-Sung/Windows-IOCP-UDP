#include "ThreadPoolTests.h"

#include <atomic>
#include <chrono>
#include <cstddef>

#include <Common/Threading/ThreadPool.h>

#include <Tests/DebugTestResult.h>
#include <Tests/TestHelpers.h>

namespace
{
	void RunStartWithZeroWorkerFailsTest(tests::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(0);
		const bool started = startResult.has_value();

		tests::Expect(result, !started, "ThreadPool: start with zero worker fails");
		tests::Expect(result, !threadPool.IsRunning(), "ThreadPool: zero worker pool is not running");
		tests::Expect(result, threadPool.GetWorkerThreadCount() == 0, "ThreadPool: zero worker count");
	}

	void RunStartAndStopTest(tests::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(2);
		const bool started = startResult.has_value();

		tests::Expect(result, started, "ThreadPool: start succeeds");
		tests::Expect(result, threadPool.IsRunning(), "ThreadPool: running after start");
		tests::Expect(result, threadPool.IsAcceptingTasks(), "ThreadPool: accepting tasks after start");
		tests::Expect(result, threadPool.GetWorkerThreadCount() == 2, "ThreadPool: worker count after start");

		threadPool.Stop();

		tests::Expect(result, !threadPool.IsRunning(), "ThreadPool: not running after stop");
		tests::Expect(result, !threadPool.IsAcceptingTasks(), "ThreadPool: not accepting tasks after stop");
		tests::Expect(result, threadPool.GetWorkerThreadCount() == 0, "ThreadPool: worker count after stop");
		tests::Expect(result, threadPool.GetPendingTaskCount() == 0, "ThreadPool: pending task count after stop");
	}

	void RunStartTwiceFailsTest(tests::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;

		const common::threading::ThreadPool::StartResult firstStartResult = threadPool.Start(1);
		const common::threading::ThreadPool::StartResult secondStartResult = threadPool.Start(1);

		const bool firstStarted = firstStartResult.has_value();
		const bool secondStarted = secondStartResult.has_value();

		tests::Expect(result, firstStarted, "ThreadPool: first start succeeds");
		tests::Expect(result, !secondStarted, "ThreadPool: second start fails");
		tests::Expect(result, threadPool.GetWorkerThreadCount() == 1, "ThreadPool: worker count after second start");

		threadPool.Stop();
	}

	void RunEmptyTaskRejectedTest(tests::DebugTestResult& result)
	{
		common::threading::ThreadPool threadPool;

		const common::threading::ThreadPool::StartResult startResult = threadPool.Start(1);
		const bool started = startResult.has_value();
		const bool enqueued = threadPool.Enqueue({});

		tests::Expect(result, started, "ThreadPool: empty task test start succeeds");
		tests::Expect(result, !enqueued, "ThreadPool: empty task rejected");

		threadPool.Stop();
	}

	void RunEnqueueAfterStopRejectedTest(tests::DebugTestResult& result)
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

		tests::Expect(result, started, "ThreadPool: enqueue after stop start succeeds");
		tests::Expect(result, !enqueued, "ThreadPool: enqueue after stop rejected");
	}

	void RunSingleTaskExecutesTest(tests::DebugTestResult& result)
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

		tests::Expect(result, started, "ThreadPool: single task start succeeds");
		tests::Expect(result, enqueued, "ThreadPool: single task enqueue succeeds");
		tests::Expect(result, completed, "ThreadPool: single task completes");
		tests::Expect(result, executedCount.load() == 1, "ThreadPool: single task executed count");

		threadPool.Stop();
	}

	void RunManyTasksExecuteTest(tests::DebugTestResult& result)
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

		tests::Expect(result, started, "ThreadPool: many task start succeeds");
		tests::Expect(result, allEnqueued, "ThreadPool: many task enqueue succeeds");
		tests::Expect(result, completed, "ThreadPool: many task completes");
		tests::Expect(result, executedCount.load() == taskCount, "ThreadPool: many task executed count");

		threadPool.Stop();
	}

	void RunTaskExceptionDoesNotStopWorkerTest(tests::DebugTestResult& result)
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

		tests::Expect(result, started, "ThreadPool: exception task start succeeds");
		tests::Expect(result, firstEnqueued, "ThreadPool: exception task enqueue succeeds");
		tests::Expect(result, secondEnqueued, "ThreadPool: next task enqueue succeeds");
		tests::Expect(result, completed, "ThreadPool: worker continues after exception");
		tests::Expect(result, executedCount.load() == 1, "ThreadPool: post exception task executed");

		threadPool.Stop();
	}

	void RunStopAfterDrainExecutesPendingTasksTest(tests::DebugTestResult& result)
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

		tests::Expect(result, started, "ThreadPool: drain start succeeds");
		tests::Expect(result, allEnqueued, "ThreadPool: drain enqueue succeeds");
		tests::Expect(result, executedCount.load() == taskCount, "ThreadPool: drain executes all tasks");
		tests::Expect(result, !threadPool.IsRunning(), "ThreadPool: drain not running after stop");
		tests::Expect(result, !threadPool.IsAcceptingTasks(), "ThreadPool: drain not accepting tasks");
		tests::Expect(result, threadPool.GetWorkerThreadCount() == 0, "ThreadPool: drain worker count after stop");
		tests::Expect(result, threadPool.GetPendingTaskCount() == 0, "ThreadPool: drain pending count after stop");
	}

	void RunStopAfterDrainRejectsNewTasksTest(tests::DebugTestResult& result)
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

		tests::Expect(result, started, "ThreadPool: drain reject start succeeds");
		tests::Expect(result, !enqueued, "ThreadPool: drain rejects new task after stop");
	}
}

namespace tests::threading
{
	tests::DebugTestResult RunThreadPoolTests()
	{
		tests::DebugTestResult result{};

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