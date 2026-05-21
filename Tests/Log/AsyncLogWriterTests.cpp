#include "AsyncLogWriterTests.h"

#include <chrono>

#include <Common/Log/AsyncLogWriter.h>
#include <Common/Log/LogLevel.h>

#include <Tests/DebugTestResult.h>
#include <Tests/TestHelpers.h>

namespace
{
	void RunStartStopTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		tests::Expect(result, started, "AsyncLogWriter: start succeeds");
		tests::Expect(result, logWriter.IsStarted(), "AsyncLogWriter: started after start");

		logWriter.Stop();

		tests::Expect(result, !logWriter.IsStarted(), "AsyncLogWriter: stopped after stop");
		tests::Expect(result, logWriter.GetPendingTaskCount() == 0, "AsyncLogWriter: pending count after stop");
	}

	void RunStartWithZeroWorkerFailsTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(0);
		const bool started = startResult.has_value();

		tests::Expect(result, !started, "AsyncLogWriter: start with zero worker fails");
		tests::Expect(result, !logWriter.IsStarted(), "AsyncLogWriter: zero worker not started");
	}

	void RunStartTwiceFailsTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult firstStartResult = logWriter.Start(1);
		const common::log::AsyncLogWriter::StartResult secondStartResult = logWriter.Start(1);

		const bool firstStarted = firstStartResult.has_value();
		const bool secondStarted = secondStartResult.has_value();

		tests::Expect(result, firstStarted, "AsyncLogWriter: first start succeeds");
		tests::Expect(result, !secondStarted, "AsyncLogWriter: second start fails");

		logWriter.Stop();
	}

	void RunLogBeforeStartFailsTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const bool logged = logWriter.Info("message before start");

		tests::Expect(result, !logged, "AsyncLogWriter: log before start fails");
	}

	void RunLogAfterStopFailsTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		logWriter.Stop();

		const bool logged = logWriter.Info("message after stop");

		tests::Expect(result, started, "AsyncLogWriter: log after stop start succeeds");
		tests::Expect(result, !logged, "AsyncLogWriter: log after stop fails");
	}

	void RunLogEnqueueSucceedsTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		const bool logged = logWriter.Info("async log test");

		const bool drained = tests::WaitUntil(
			[&logWriter]()
			{
				return logWriter.GetPendingTaskCount() == 0;
			},
			std::chrono::milliseconds(1000)
		);

		tests::Expect(result, started, "AsyncLogWriter: enqueue start succeeds");
		tests::Expect(result, logged, "AsyncLogWriter: enqueue succeeds");
		tests::Expect(result, drained, "AsyncLogWriter: queue drained");

		logWriter.Stop();
	}

	void RunMinimumLogLevelFiltersTest(tests::DebugTestResult& result)
	{
		common::log::AsyncLogWriter logWriter;

		const common::log::AsyncLogWriter::StartResult startResult = logWriter.Start(1);
		const bool started = startResult.has_value();

		logWriter.SetMinimumLogLevel(common::log::LogLevel::Warning);

		const bool infoLogged = logWriter.Info("filtered info log");
		const bool warningLogged = logWriter.Warning("visible warning log");

		tests::Expect(result, started, "AsyncLogWriter: filter start succeeds");
		tests::Expect(result, !infoLogged, "AsyncLogWriter: info filtered");
		tests::Expect(result, warningLogged, "AsyncLogWriter: warning accepted");

		logWriter.Stop();
	}
}

namespace tests::log
{
	tests::DebugTestResult RunAsyncLogWriterTests()
	{
		tests::DebugTestResult result{};

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