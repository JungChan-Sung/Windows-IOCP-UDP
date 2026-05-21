#include "AsyncLogWriterTests.h"

#include <chrono>
#include <functional>
#include <thread>

#include <Common/Diagnostics/DebugTestResult.h>
#include <Common/Log/AsyncLogWriter.h>
#include <Common/Log/LogLevel.h>

namespace
{
	[[nodiscard]] bool WaitUntil(
		const std::function<bool()>& predicate,
		std::chrono::milliseconds timeout
	)
	{
		const auto startTime = std::chrono::steady_clock::now();

		while (!predicate())
		{
			if (std::chrono::steady_clock::now() - startTime >= timeout)
			{
				return false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		return true;
	}

	void RunStartStopTest(common::diagnostics::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		common::diagnostics::Expect(result, started, "AsyncLogWriter: start succeeds");
		common::diagnostics::Expect(result, logWriter.IsStarted(), "AsyncLogWriter: started after start");

		logWriter.Stop();

		common::diagnostics::Expect(result, !logWriter.IsStarted(), "AsyncLogWriter: stopped after stop");
		common::diagnostics::Expect(result, logWriter.GetPendingTaskCount() == 0, "AsyncLogWriter: pending count after stop");
	}

	void RunStartWithZeroWorkerFailsTest(common::diagnostics::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(0);
		const bool started = startResult.has_value();

		common::diagnostics::Expect(result, !started, "AsyncLogWriter: start with zero worker fails");
		common::diagnostics::Expect(result, !logWriter.IsStarted(), "AsyncLogWriter: zero worker not started");
	}

	void RunStartTwiceFailsTest(common::diagnostics::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult firstStartResult = logWriter.Start(1);
		const common::log::AsyncLogWriter::StartResult secondStartResult = logWriter.Start(1);

		const bool firstStarted = firstStartResult.has_value();
		const bool secondStarted = secondStartResult.has_value();

		common::diagnostics::Expect(result, firstStarted, "AsyncLogWriter: first start succeeds");
		common::diagnostics::Expect(result, !secondStarted, "AsyncLogWriter: second start fails");

		logWriter.Stop();
	}

	void RunLogBeforeStartFailsTest(common::diagnostics::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const bool logged = logWriter.Info("message before start");

		common::diagnostics::Expect(result, !logged, "AsyncLogWriter: log before start fails");
	}

	void RunLogAfterStopFailsTest(common::diagnostics::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		logWriter.Stop();

		const bool logged = logWriter.Info("message after stop");

		common::diagnostics::Expect(result, started, "AsyncLogWriter: log after stop start succeeds");
		common::diagnostics::Expect(result, !logged, "AsyncLogWriter: log after stop fails");
	}

	void RunLogEnqueueSucceedsTest(common::diagnostics::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		const bool logged = logWriter.Info("async log test");

		const bool drained = WaitUntil(
			[&logWriter]()
			{
				return logWriter.GetPendingTaskCount() == 0;
			},
			std::chrono::milliseconds(1000)
		);

		common::diagnostics::Expect(result, started, "AsyncLogWriter: enqueue start succeeds");
		common::diagnostics::Expect(result, logged, "AsyncLogWriter: enqueue succeeds");
		common::diagnostics::Expect(result, drained, "AsyncLogWriter: queue drained");

		logWriter.Stop();
	}

	void RunMinimumLogLevelFiltersTest(common::diagnostics::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		logWriter.SetMinimumLogLevel(common::log::LogLevel::Warning);

		const bool infoLogged = logWriter.Info("filtered info log");
		const bool warningLogged = logWriter.Warning("visible warning log");

		common::diagnostics::Expect(result, started, "AsyncLogWriter: filter start succeeds");
		common::diagnostics::Expect(result, !infoLogged, "AsyncLogWriter: info filtered");
		common::diagnostics::Expect(result, warningLogged, "AsyncLogWriter: warning accepted");

		logWriter.Stop();
	}
}

namespace tests::log
{
	common::diagnostics::DebugTestResult RunAsyncLogWriterTests()
	{
		common::diagnostics::DebugTestResult result{};

		RunStartStopTest(result);
		RunStartWithZeroWorkerFailsTest(result);
		RunStartTwiceFailsTest(result);
		RunLogBeforeStartFailsTest(result);
		RunLogAfterStopFailsTest(result);
		RunLogEnqueueSucceedsTest(result);
		RunMinimumLogLevelFiltersTest(result);

		return result;
	}
}